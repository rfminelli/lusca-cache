
/*
 * $Id$
 *
 * DEBUG: section 11    Hypertext Transfer Protocol (HTTP)
 * AUTHOR: Harvest Derived
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

/*
 * Anonymizing patch by lutz@as-node.jena.thur.de
 * have a look into http-anon.c to get more informations.
 */

#include "squid.h"

static const char *const crlf = "\r\n";

static CWCB httpSendComplete;
static CWCB httpSendRequestEntry;

static PF httpReadReply;
static void httpSendRequest(HttpStateData *);
static PF httpStateFree;
static PF httpTimeout;
static void httpCacheNegatively(StoreEntry *);
static void httpMakePrivate(StoreEntry *);
static void httpMakePublic(StoreEntry *);
static int httpCachableReply(HttpStateData *);
static void httpMaybeRemovePublic(StoreEntry *, HttpReply *);
static int peer_supports_connection_pinning(HttpStateData * httpState);

static int
httpUrlHostsMatch(const char *url1, const char *url2)
{
    const char *host1 = strchr(url1, ':');
    const char *host2 = strchr(url2, ':');

    if (host1 && host2) {
	do {
	    ++host1;
	    ++host2;
	} while (*host1 == '/' && *host2 == '/');

	if (!*host1) {
	    return (0);
	}
	while (*host1 && *host1 != '/' && *host1 == *host2) {
	    ++host1;
	    ++host2;
	}
	return (*host1 == *host2);
    }
    return (0);
}

static void
httpStateFree(int fd, void *data)
{
    HttpStateData *httpState = data;
#if DELAY_POOLS
    delayClearNoDelay(fd);
#endif
    if (httpState == NULL)
	return;
    if (httpState->body_buf) {
	requestAbortBody(httpState->orig_request);
	if (httpState->body_buf) {
	    memFree(httpState->body_buf, MEM_8K_BUF);
	    httpState->body_buf = NULL;
	}
    }
    storeUnlockObject(httpState->entry);
    if (!memBufIsNull(&httpState->reply_hdr)) {
	memBufClean(&httpState->reply_hdr);
    }
    requestUnlink(httpState->request);
    requestUnlink(httpState->orig_request);
    httpState->request = NULL;
    httpState->orig_request = NULL;
    stringClean(&httpState->chunkhdr);
    cbdataFree(httpState);
}

int
httpCachable(method_t * method)
{

    return (method->flags.cachable);
}

static void
httpTimeout(int fd, void *data)
{
    HttpStateData *httpState = data;
    StoreEntry *entry = httpState->entry;
    debug(11, 4) ("httpTimeout: FD %d: '%s'\n", fd, storeUrl(entry));
    if (entry->store_status == STORE_PENDING) {
	fwdFail(httpState->fwd,
	    errorCon(ERR_READ_TIMEOUT, HTTP_GATEWAY_TIMEOUT, httpState->fwd->request));
    }
    comm_close(fd);
}

/* This object can be cached for a long time */
static void
httpMakePublic(StoreEntry * entry)
{
    if (EBIT_TEST(entry->flags, ENTRY_CACHABLE))
	storeSetPublicKey(entry);
}

/* This object should never be cached at all */
static void
httpMakePrivate(StoreEntry * entry)
{
    storeExpireNow(entry);
    storeReleaseRequest(entry);	/* delete object when not used */
    /* storeReleaseRequest clears ENTRY_CACHABLE flag */
}

/* This object may be negatively cached */
static void
httpCacheNegatively(StoreEntry * entry)
{
    storeNegativeCache(entry);
    if (EBIT_TEST(entry->flags, ENTRY_CACHABLE))
	storeSetPublicKey(entry);
    if (entry->expires <= squid_curtime)
	storeRelease(entry);
}

static void
httpRemovePublicByHeader(request_t * req, HttpReply * reply, http_hdr_type header)
{
    const char *hdrUrl;
    char *absUrl;

    absUrl = NULL;
    hdrUrl = httpHeaderGetStr(&reply->header, header);
    if (hdrUrl == NULL) {
	return;
    }
    /*
     * If the URL is relative, make it absolute so we can find it.
     * If it's absolute, make sure the host parts match to avoid DOS attacks
     * as per RFC 2616 13.10.
     */
    if (urlIsRelative(hdrUrl)) {
	absUrl = urlMakeAbsolute(req, hdrUrl);
	if (absUrl != NULL) {
	    hdrUrl = absUrl;
	}
    } else if (!httpUrlHostsMatch(hdrUrl, urlCanonical(req))) {
	return;
    }
    storePurgeEntriesByUrl(req, hdrUrl);

    if (absUrl != NULL) {
	safe_free(absUrl);
    }
}

static void
httpMaybeRemovePublic(StoreEntry * e, HttpReply * reply)
{
    int status;
    int remove = 0;
    int forbidden = 0;
    StoreEntry *pe;
    request_t *req;
    const char *reqUrl;

    status = reply->sline.status;
    switch (status) {
    case HTTP_OK:
    case HTTP_NON_AUTHORITATIVE_INFORMATION:
    case HTTP_MULTIPLE_CHOICES:
    case HTTP_MOVED_PERMANENTLY:
    case HTTP_MOVED_TEMPORARILY:
    case HTTP_GONE:
    case HTTP_NOT_FOUND:
	remove = 1;
	break;
    case HTTP_FORBIDDEN:
    case HTTP_METHOD_NOT_ALLOWED:
	forbidden = 1;
	break;
#if WORK_IN_PROGRESS
    case HTTP_UNAUTHORIZED:
	forbidden = 1;
	break;
#endif
    default:
#if QUESTIONABLE
	/*
	 * Any 2xx response should eject previously cached entities...
	 */
	if (status >= 200 && status < 300)
	    remove = 1;
#endif
	break;
    }
    if (!remove && !forbidden)
	return;
    if (EBIT_TEST(e->flags, KEY_PRIVATE)) {
	assert(e->mem_obj);
	if (e->mem_obj->request)
	    pe = storeGetPublicByRequest(e->mem_obj->request);
	else
	    pe = storeGetPublic(e->mem_obj->url, e->mem_obj->method);
	if (pe != NULL) {
	    assert(e != pe);
	    storeRelease(pe);
	}
    }
    /*
     * Also remove any cached HEAD response in case the object has
     * changed.
     */
    if (e->mem_obj->request)
	pe = storeGetPublicByRequestMethodCode(e->mem_obj->request, METHOD_HEAD);
    else
	pe = storeGetPublicByCode(e->mem_obj->url, METHOD_HEAD);
    if (pe != NULL && e != pe) {
#if USE_HTCP
	neighborsHtcpClear(e, NULL, e->mem_obj->request, urlMethodGetKnownByCode(METHOD_HEAD), HTCP_CLR_INVALIDATION);
#endif
	storeRelease(pe);
    }
    if (forbidden)
	return;
    if (e->mem_obj->method->flags.purges_all && status < 400) {
	req = e->mem_obj->request;
	reqUrl = urlCanonical(req);
	debug(88, 5) ("httpMaybeRemovePublic: purging due to %s %s\n", req->method->string, reqUrl);
	storePurgeEntriesByUrl(req, reqUrl);
	httpRemovePublicByHeader(req, reply, HDR_LOCATION);
	httpRemovePublicByHeader(req, reply, HDR_CONTENT_LOCATION);
    }
}

static int
httpCachableReply(HttpStateData * httpState)
{
    HttpReply *rep = httpState->entry->mem_obj->reply;
    HttpHeader *hdr = &rep->header;
    const int cc_mask = (rep->cache_control) ? rep->cache_control->mask : 0;
    const char *v;
#if HTTP_VIOLATIONS
    const refresh_t *R = NULL;
    /* This strange looking define first looks up the refresh pattern
     * and then checks if the specified flag is set. The main purpose
     * of this is to simplify the refresh pattern lookup
     */
#define REFRESH_OVERRIDE(flag) \
	((R = (R ? R : refreshLimits(httpState->entry->mem_obj->url))) , \
	(R && R->flags.flag))
#else
#define REFRESH_OVERRIDE(field)	0
#endif
    if (EBIT_TEST(cc_mask, CC_PRIVATE) && !REFRESH_OVERRIDE(ignore_private))
	return 0;
    if (EBIT_TEST(cc_mask, CC_NO_CACHE) && !REFRESH_OVERRIDE(ignore_no_cache))
	return 0;
    if (EBIT_TEST(cc_mask, CC_NO_STORE))
	return 0;
    if (httpState->request->flags.auth_sent) {
	/*
	 * Responses to requests with authorization may be cached
	 * only if a Cache-Control: public reply header is present.
	 * RFC 2068, sec 14.9.4
	 */
	if (!EBIT_TEST(cc_mask, CC_PUBLIC) && !REFRESH_OVERRIDE(ignore_auth))
	    return 0;
    }
    /* Pragma: no-cache in _replies_ is not documented in HTTP,
     * but servers like "Active Imaging Webcast/2.0" sure do use it */
    if (httpHeaderHas(hdr, HDR_PRAGMA)) {
	String s = httpHeaderGetList(hdr, HDR_PRAGMA);
	const int no_cache = strListIsMember(&s, "no-cache", ',');
	stringClean(&s);
	if (no_cache && !REFRESH_OVERRIDE(ignore_no_cache))
	    return 0;
    }
    /*
     * The "multipart/x-mixed-replace" content type is used for
     * continuous push replies.  These are generally dynamic and
     * probably should not be cachable
     */
    if ((v = httpHeaderGetStr(hdr, HDR_CONTENT_TYPE)))
	if (!strncasecmp(v, "multipart/x-mixed-replace", 25))
	    return 0;
    switch (httpState->entry->mem_obj->reply->sline.status) {
	/* Responses that are cacheable */
    case HTTP_OK:
    case HTTP_NON_AUTHORITATIVE_INFORMATION:
    case HTTP_MULTIPLE_CHOICES:
    case HTTP_MOVED_PERMANENTLY:
    case HTTP_GONE:
	/*
	 * Don't cache objects that need to be refreshed on next request,
	 * unless we know how to refresh it.
	 */
	if (!refreshIsCachable(httpState->entry))
	    return 0;
	/* don't cache objects from peers w/o LMT, Date, or Expires */
	/* check that is it enough to check headers @?@ */
	if (rep->date > -1)
	    return 1;
	else if (rep->last_modified > -1)
	    return 1;
	else if (!httpState->peer)
	    return 1;
	/* @?@ (here and 302): invalid expires header compiles to squid_curtime */
	else if (rep->expires > -1)
	    return 1;
	else
	    return 0;
	/* NOTREACHED */
	break;
	/* Responses that only are cacheable if the server says so */
    case HTTP_MOVED_TEMPORARILY:
	if (rep->expires > rep->date && rep->date > 0)
	    return 1;
	else
	    return 0;
	/* NOTREACHED */
	break;
	/* Errors can be negatively cached */
    case HTTP_NO_CONTENT:
    case HTTP_USE_PROXY:
    case HTTP_BAD_REQUEST:
    case HTTP_FORBIDDEN:
    case HTTP_NOT_FOUND:
    case HTTP_METHOD_NOT_ALLOWED:
    case HTTP_REQUEST_URI_TOO_LONG:
    case HTTP_INTERNAL_SERVER_ERROR:
    case HTTP_NOT_IMPLEMENTED:
    case HTTP_BAD_GATEWAY:
    case HTTP_SERVICE_UNAVAILABLE:
    case HTTP_GATEWAY_TIMEOUT:
	return -1;
	/* NOTREACHED */
	break;
	/* Some responses can never be cached */
    case HTTP_PARTIAL_CONTENT:	/* Not yet supported */
    case HTTP_SEE_OTHER:
    case HTTP_NOT_MODIFIED:
    case HTTP_UNAUTHORIZED:
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
    case HTTP_INVALID_HEADER:	/* Squid header parsing error */
    case HTTP_HEADER_TOO_LARGE:
    default:			/* Unknown status code */
	return 0;
	/* NOTREACHED */
	break;
    }
    /* NOTREACHED */
}

/*
 * For Vary, store the relevant request headers as 
 * virtual headers in the reply
 * Returns false if the variance cannot be stored
 */
const char *
httpMakeVaryMark(request_t * request, HttpReply * reply)
{
    String vary = StringNull, hdr;
    const char *pos = NULL;
    const char *item;
    const char *value;
    int ilen;
    String vstr = StringNull;

    stringClean(&vstr);
    hdr = httpHeaderGetList(&reply->header, HDR_VARY);
    if (strIsNotNull(hdr))
	strListAddStr(&vary, strBuf2(hdr), strLen2(hdr), ',');
    stringClean(&hdr);
#if X_ACCELERATOR_VARY
    hdr = httpHeaderGetList(&reply->header, HDR_X_ACCELERATOR_VARY);
    if (strIsNotNull(hdr))
	strListAddStr(&vary, strBuf2(hdr), strLen2(hdr), ',');
    stringClean(&hdr);
#endif
    while (strListGetItem(&vary, ',', &item, &ilen, &pos)) {
	char *name = xmalloc(ilen + 1);
	xstrncpy(name, item, ilen + 1);
	Tolower(name);
	if (strcmp(name, "accept-encoding") == 0) {
	    aclCheck_t checklist;
	    memset(&checklist, 0, sizeof(checklist));
	    checklist.request = request;
	    checklist.reply = reply;
	    if (Config.accessList.vary_encoding && aclCheckFast(Config.accessList.vary_encoding, &checklist)) {
		stringClean(&request->vary_encoding);
		request->vary_encoding = httpHeaderGetStrOrList(&request->header, HDR_ACCEPT_ENCODING);
		strCat(request->vary_encoding, "");
	    }
	}
	if (strcmp(name, "*") == 0) {
	    /* Can not handle "Vary: *" efficiently, bail out making the response not cached */
	    safe_free(name);
	    stringClean(&vary);
	    stringClean(&vstr);
	    break;
	}
	strListAdd(&vstr, name, ',');
	hdr = httpHeaderGetByName(&request->header, name);
	safe_free(name);
	value = strBuf(hdr);
	if (value) {
	    value = rfc1738_escape_part(value);
	    stringAppend(&vstr, "=\"", 2);
	    stringAppend(&vstr, value, strlen(value));
	    stringAppend(&vstr, "\"", 1);
	}
	stringClean(&hdr);
    }
    safe_free(request->vary_hdr);
    safe_free(request->vary_headers);
    if (strIsNotNull(vary) && strIsNotNull(vstr)) {
	request->vary_hdr = stringDupToC(&vary);
	request->vary_headers = stringDupToC(&vstr);
    }
    debug(11, 3) ("httpMakeVaryMark: %.*s\n", strLen2(vstr), strBuf2(vstr));
    stringClean(&vary);
    stringClean(&vstr);
    return request->vary_headers;
}

static void
httpSetHttp09Header(HttpStateData *httpState, HttpReply *reply)
{
	char *t, *t2;

	debug(11, 3) ("httpSetHttp09Header: Non-HTTP-compliant header: '%s'\n", httpState->reply_hdr.buf);
	t = xstrdup(httpState->reply_hdr.buf);
	t2 = strchr(t, '\n');
	if (t2)
	    *t2 = '\0';
	t2 = strchr(t, '\r');
	if (t2)
	    *t2 = '\0';
	httpHeaderPutStr(&reply->header, HDR_X_HTTP09_FIRST_LINE, t);
	safe_free(t);
}

static void
httpAppendReplyHeader(HttpStateData * httpState, const char *buf, int size)
{
    assert(! memBufIsNull(&httpState->reply_hdr));
    memBufAppend(&httpState->reply_hdr, buf, size);
}

/*
 * Handle a (mostly!) valid response; setup internal header structures
 * in preparation for handling the reply.
 *
 * This function is a mash of both transfer related information (TE,
 * keepalive, etc) and caching related information. Ideally they'd be
 * completely seperate so a non-caching HTTP client could be implemented
 * and used elsewhere; that should happen later on and stuffed into
 * a top-level library separate from the cache codebase.
 */
static int
httpReplySetupStuff(HttpStateData *httpState)
{
    StoreEntry *entry = httpState->entry;
    HttpReply *reply = entry->mem_obj->reply;

    if (!peer_supports_connection_pinning(httpState))
	httpState->orig_request->flags.no_connection_auth = 1;
    storeTimestampsSet(entry);
    /* Check if object is cacheable or not based on reply code */
    debug(11, 3) ("httpProcessReplyHeader: HTTP CODE: %d\n", reply->sline.status);
    if (httpHeaderHas(&reply->header, HDR_TRANSFER_ENCODING)) {
	String tr = httpHeaderGetList(&reply->header, HDR_TRANSFER_ENCODING);
	const char *pos = NULL;
	const char *item = NULL;
	int ilen = 0;
	if (strListGetItem(&tr, ',', &item, &ilen, &pos)) {
	    if (ilen == 7 && strncasecmp(item, "chunked", ilen) == 0) {
		httpState->flags.chunked = 1;
		if (!strListGetItem(&tr, ',', &item, &ilen, &pos))
		    item = NULL;
	    }
	    if (item) {
		/* Can't handle other transfer-encodings */
		debug(11, 1) ("Unexpected transfer encoding '%.*s'\n", strLen2(tr), strBuf2(tr));
		reply->sline.status = HTTP_INVALID_HEADER;
		return -1;
	    }
	}
	stringClean(&tr);
	if (httpState->flags.chunked && reply->content_length >= 0) {
	    /* Can't have a content-length in chunked encoding */
	    reply->content_length = -1;
	    httpHeaderDelById(&reply->header, HDR_CONTENT_LENGTH);
	}
    }
    if (!httpState->flags.chunked) {
	/* non-chunked. Handle as one single big chunk (-1 if terminated by EOF) */
	httpState->chunk_size = httpReplyBodySize(httpState->orig_request->method, reply);
    }
    if (httpHeaderHas(&reply->header, HDR_VARY)
#if X_ACCELERATOR_VARY
	|| httpHeaderHas(&reply->header, HDR_X_ACCELERATOR_VARY)
#endif
	) {
	const char *vary = NULL;
	if (Config.onoff.cache_vary)
	    vary = httpMakeVaryMark(httpState->orig_request, reply);
	if (!vary) {
	    httpMakePrivate(entry);
	    goto no_cache;	/* XXX Would be better if this was used by the swicht statement below */
	}
	entry->mem_obj->vary_headers = xstrdup(vary);
	if (strIsNotNull(httpState->orig_request->vary_encoding))
	    entry->mem_obj->vary_encoding = stringDupToC(&httpState->orig_request->vary_encoding);
    }
    if (entry->mem_obj->old_entry)
	EBIT_CLR(entry->mem_obj->old_entry->flags, REFRESH_FAILURE);
    switch (httpCachableReply(httpState)) {
    case 1:
	httpMakePublic(entry);
	break;
    case 0:
	httpMakePrivate(entry);
	break;
    case -1:
	httpCacheNegatively(entry);
	break;
    default:
	assert(0);
	break;
    }
  no_cache:
    if (reply->cache_control) {
	if (EBIT_TEST(reply->cache_control->mask, CC_PROXY_REVALIDATE))
	    EBIT_SET(entry->flags, ENTRY_REVALIDATE);
	else if (EBIT_TEST(reply->cache_control->mask, CC_MUST_REVALIDATE))
	    EBIT_SET(entry->flags, ENTRY_REVALIDATE);
    }
    if (neighbors_do_private_keys && !Config.onoff.collapsed_forwarding)
	httpMaybeRemovePublic(entry, reply);
    if (httpState->flags.keepalive)
	if (httpState->peer)
	    httpState->peer->stats.n_keepalives_sent++;
    if (reply->keep_alive) {
	if (httpState->peer)
	    httpState->peer->stats.n_keepalives_recv++;
	if (Config.onoff.detect_broken_server_pconns && httpReplyBodySize(httpState->request->method, reply) == -1) {
	    debug(11, 1) ("httpProcessReplyHeader: Impossible keep-alive header from '%s'\n", storeUrl(entry));
	    debug(11, 2) ("GOT HTTP REPLY HDR:\n---------\n%.*s\n----------\n",
		httpState->reply_hdr.size, httpState->reply_hdr.buf);
	    httpState->flags.keepalive_broken = 1;
	}
    }
    if (reply->date > -1 && !httpState->peer) {
	int skew = abs(reply->date - squid_curtime);
	if (skew > 86400)
	    debug(11, 3) ("%s's clock is skewed by %d seconds!\n",
		httpState->request->host, skew);
    }
    return 0;
}

/* rewrite this later using new interfaces @?@ */
/*
 * Parsing starts from offset 's' until the end of the MemBuf.
 * The return value "done" is the number of bytes consumed in
 * the MemBuf, -excluding- the start offset thats being ignored.
 * This means '0' means that no bytes were consumed as headers.
 *
 * This function is a bit of a mess of a whole lot of different bits.
 * The return value is the number of bytes consumed out of the MemBuf,
 * including the starting offset.
 *
 * Actual error handling is done by looking at the status line code,
 * rather than the return value of this function.
 *
 * The current big issue is the recursive handling of the 100 support
 * inside this function. It probably shouldn't be done in here.
 */
static size_t
httpProcessReplyHeader(HttpStateData * httpState, int s)
{
    StoreEntry *entry = httpState->entry;
    size_t done;
    HttpReply *reply = entry->mem_obj->reply;
    const char *hdr_buf = httpState->reply_hdr.buf + s;		/* buffer to parse from */
    size_t hdr_len = httpState->reply_hdr.size - s;		/* length of buffer to parse from */
    size_t hdr_size;						/* actual size of reply status + headers in hdr_buf */

    Ctx ctx = ctx_enter(entry->mem_obj->url);
    debug(11, 3) ("httpProcessReplyHeader: key '%s'\n",
	storeKeyText(entry->hash.key));
    assert(! memBufIsNull(&httpState->reply_hdr));
    assert(httpState->reply_hdr_state == 0);

    /* Handle non-parsable responses as HTTP/0.9 responses - ie, no headers, just verbatim body */
    if (hdr_len > 4 && strncmp(hdr_buf, "HTTP/", 5)) {
	/* This function sets a reply header to the first line of the reply */
        httpSetHttp09Header(httpState, reply);
	/* Set state to parsed */
	httpState->reply_hdr_state += 2;
	/* No chunk size - terminated via EOF */
	httpState->chunk_size = -1;	/* Terminated by EOF */
	/* Reply header size is whatever it was -before- this data came in - which is what, exactly? */
	httpBuildVersion(&reply->sline.version, 0, 9);
	reply->sline.status = HTTP_INVALID_HEADER;
	ctx_exit(ctx);
	/* The "return 0" means "none of the data in the reply buffer is used as header; treat it all as body */
	/* So the calling code should storeAppend() it as the reply body */
	return 0;
    }

    /* Try to delineate the entire reply status + header set */
    hdr_size = headersEnd(hdr_buf, hdr_len);
    if (hdr_size)
	hdr_len = hdr_size;

    /* Is the reply buffer size (whether a full reply is contained or not) > max? */
    if (hdr_len > Config.maxReplyHeaderSize) {
	debug(11, 1) ("httpProcessReplyHeader: Too large reply header\n");
	storeAppend(entry, hdr_buf, hdr_len);
	memBufClean(&httpState->reply_hdr);	/* XXX hdr_buf / hdr_len are invalid now */
	reply->sline.status = HTTP_HEADER_TOO_LARGE;
	httpState->reply_hdr_state += 2;
	ctx_exit(ctx);
        /* XXX why return hdr_len here when the memBuf used has been cleaned? */
	//return hdr_len;
        return 0;
    }

    /* Only return "headers not complete" if EOF hasn't yet been read on the socket ? */
    /* headers can be incomplete only if object still arriving */
    /* XXX Should make sure this code does what is intended/required!!! */
    if (!hdr_size) {
	if (httpState->eof)
	    hdr_size = hdr_len;
	else {
	    ctx_exit(ctx);
	    return hdr_len;	/* headers not complete */
	}
    }
    /* Free any existing variant information before we add our own */
    safe_free(entry->mem_obj->vary_headers);
    safe_free(entry->mem_obj->vary_encoding);

    httpState->reply_hdr_state++;
    assert(httpState->reply_hdr_state == 1);
    httpState->reply_hdr_state++;
    debug(11, 9) ("GOT HTTP REPLY HDR:\n---------\n%.*s\n----------\n", (int) hdr_size, hdr_buf);

    /* Parse headers into reply structure */
    /* what happens if we fail to parse here? */
    httpReplyParse(reply, hdr_buf, hdr_size);

    /*
     * how many bytes in the reply buffer are left over? The caller will then use those
     * bytes as part of the reply body, not status+headers
     */
    done = hdr_size;

    /* Append the reply status and header, unparsed, to the store object */
    storeAppend(entry, httpState->reply_hdr.buf + s, hdr_size);

    /*
     * If the reply was invalid, and it didn't pass HTTP/0.9 muster, clean the reply header data
     * entirely and return how much of the data was consumed. Hopefully this means the data
     * will simply be discarded by the caller and whatever is left is for the next reply?
     * (But we don't currently support pipelining!)
     */
    if (reply->sline.status >= HTTP_INVALID_HEADER) {
	debug(11, 3) ("httpProcessReplyHeader: Non-HTTP-compliant header: '%.*s'\n", (int) hdr_size, hdr_buf);
	memBufClean(&httpState->reply_hdr);
	ctx_exit(ctx);
        /* XXX why return hdr_len here when the memBuf used has been cleaned? */
	//return done;
        return 0;
    }

    /*
     * After this point - "done" is set indicating how much data from the passed in buffer
     * has been used as reply status+headers (and thus how much is left for the reply body);
     * the reply has been parsed (but not error checked at all for some silly reason? :);
     * HTTP/0.9 replies have been dealt with; 1xx status code messages have been skipped in
     * the HTTP reply flow; the request status + headers have been appended to the store object.
     * The reply -may- still be invalidated, but "done" doesn't change, nor is any other
     * value returned.
     */

    /*
     * So the below code is purely fiddling around with setting up the actual reply state
     * based on stuff in the reply.
     */
    (void) httpReplySetupStuff(httpState);

    ctx_exit(ctx);
#if HEADERS_LOG
    headersLog(1, 0, httpState->request->method, reply);
#endif
    return done;
}

/* Small helper function to verify if connection pinning is supported or not
 */
static int
peer_supports_connection_pinning(HttpStateData * httpState)
{
    const HttpReply *rep = httpState->entry->mem_obj->reply;
    const HttpHeader *hdr = &rep->header;
    const request_t *req = httpState->request;
    int rc;
    String header;

    if (!httpState->peer)
	return 1;

    if (!httpState->peer->connection_auth)
	return 0;

    if (rep->sline.status != HTTP_UNAUTHORIZED)
	return 1;

    if (httpState->peer->connection_auth == 1)
	return 1;

    if (httpState->peer->options.originserver)
	return 1;

    if (req->flags.pinned)
	return 1;

    if (!httpHeaderHas(hdr, HDR_PROXY_SUPPORT))
	return 0;

    header = httpHeaderGetStrOrList(hdr, HDR_PROXY_SUPPORT);
    /* XXX This ought to be done in a case-insensitive manner */
    rc = (strStr(header, "Session-Based-Authentication") != NULL);
    stringClean(&header);

    return rc;
}

static void
httpAppendBody(HttpStateData * httpState, const char *buf, ssize_t len, int buffer_filled)
{
    StoreEntry *entry = httpState->entry;
    const request_t *request = httpState->request;
    const request_t *orig_request = httpState->orig_request;
    struct in_addr *client_addr = NULL;
    u_short client_port = 0;
    int fd = httpState->fd;
    int complete = httpState->eof;
    int keep_alive = !httpState->eof;
    storeBuffer(entry);
    if (len == 0 && httpState->eof && httpState->flags.chunked) {
	fwdFail(httpState->fwd, errorCon(ERR_INVALID_RESP, HTTP_BAD_GATEWAY, httpState->fwd->request));
	comm_close(fd);
	return;
    }
    while (len > 0) {
	if (httpState->chunk_size > 0) {
	    size_t size = len;
	    if (size > httpState->chunk_size)
		size = httpState->chunk_size;
	    httpState->chunk_size -= size;
	    storeAppend(httpState->entry, buf, size);
	    buf += size;
	    len -= size;
	} else if (httpState->chunk_size < 0) {
	    /* non-chunked without content-length */
	    storeAppend(httpState->entry, buf, len);
	    len = 0;
	} else if (httpState->flags.chunked) {
	    char *eol = memchr(buf, '\n', len);
	    size_t size = eol - buf + 1;
	    if (!eol)
		size = len;
	    stringAppend(&httpState->chunkhdr, buf, size);
	    buf += size;
	    len -= size;
	    if (strLen(httpState->chunkhdr) > 256) {
		debug(11, 1) ("Oversized chunk header on port %d, url %s\n", comm_local_port(fd), entry->mem_obj->url);
		fwdFail(httpState->fwd, errorCon(ERR_INVALID_RESP, HTTP_BAD_GATEWAY, httpState->fwd->request));
		comm_close(fd);
		return;
	    }
	    if (eol) {
		if (!httpState->flags.trailer) {
		    /* chunk header */
		    char *end = NULL;
		    int badchunk = 0;
		    int emptychunk = 0;
		    debug(11, 3) ("Chunk header '%.*s'\n", strLen2(httpState->chunkhdr), strBuf2(httpState->chunkhdr));
		    errno = 0;
		    httpState->chunk_size = strto_off_t(strBuf(httpState->chunkhdr), &end, 16);
		    if (errno)
			badchunk = 1;
		    else if (end == strBuf(httpState->chunkhdr))
			emptychunk = 1;
		    while (end && (*end == '\r' || *end == ' ' || *end == '\t'))
			end++;
		    if (httpState->chunk_size < 0 || badchunk || !end || (*end != '\n' && *end != ';')) {
			debug(11, 1) ("Invalid chunk header '%.*s'\n", strLen2(httpState->chunkhdr), strBuf2(httpState->chunkhdr));
			fwdFail(httpState->fwd, errorCon(ERR_INVALID_RESP, HTTP_BAD_GATEWAY, httpState->fwd->request));
			comm_close(fd);
			return;
		    }
		    if (emptychunk)
			continue;	/* Skip blank lines */
		    debug(11, 2) ("Chunk size %" PRINTF_OFF_T "\n", httpState->chunk_size);
		    if (httpState->chunk_size == 0) {
			debug(11, 3) ("Processing trailer\n");
			httpState->flags.trailer = 1;
		    }
		} else {
		    /* trailer */
		    const char *p = strBuf(httpState->chunkhdr);
		    while (*p == '\r')
			p++;
		    if (*p == '\n') {
			complete = 1;
			debug(11, 2) ("Chunked response complete\n");
		    }
		}
		stringReset(&httpState->chunkhdr, NULL);
	    }
	} else {
	    /* Don't know what to do with this data. Bail out */
	    break;
	}
	if (EBIT_TEST(entry->flags, ENTRY_ABORTED)) {
	    /*
	     * the above storeAppend() call could ABORT this entry,
	     * in that case, the server FD should already be closed.
	     * there's nothing for us to do.
	     */
	    return;
	}
    }
    storeBufferFlush(entry);
    if (EBIT_TEST(entry->flags, ENTRY_ABORTED)) {
	/*
	 * the above storeBufferFlush() call could ABORT this entry,
	 * in that case, the server FD should already be closed.
	 * there's nothing for us to do.
	 */
	return;
    }
    if (!httpState->chunk_size && !httpState->flags.chunked)
	complete = 1;
    if (!complete && len == 0) {
	/* Wait for more data or EOF condition */
	if (httpState->flags.keepalive_broken) {
	    commSetTimeout(fd, 10, NULL, NULL);
	} else {
	    commSetTimeout(fd, Config.Timeout.read, NULL, NULL);
	}
	commSetSelect(fd, COMM_SELECT_READ, httpReadReply, httpState, 0);
	return;
    }
    /* Is it a incomplete reply? */
    if (httpState->chunk_size > 0) {
	debug(11, 2) ("Short response from '%s' on port %d. Expecting %" PRINTF_OFF_T " octets more\n", storeUrl(entry), comm_local_port(fd), httpState->chunk_size);
	comm_close(fd);
	return;
    }
    /*
     * Verify that the connection is clean
     */
    if (len == 0 && buffer_filled >= 0) {
	char buf2[4];
	CommStats.syscalls.sock.reads++;
	len = FD_READ_METHOD(fd, buf2, sizeof(buf2));
	if ((len < 0 && !ignoreErrno(errno)) || len == 0) {
	    keep_alive = 0;
	}
    }
    if (len > 0) {
	debug(11, Config.onoff.relaxed_header_parser <= 0 || keep_alive ? 1 : 2)
	    ("httpReadReply: Excess data from \"%s %s\"\n",
	    orig_request->method->string,
	    storeUrl(entry));
	comm_close(fd);
	return;
    }
    /*
     * Verified and done with the reply
     */

    /*
     * If we didn't send a keep-alive request header, then this
     * can not be a persistent connection.
     */
    if (!httpState->flags.keepalive)
	keep_alive = 0;
    /*
     * If we haven't sent the whole request then this can not be a persistent
     * connection.
     */
    if (!httpState->flags.request_sent) {
	debug(11, 1) ("httpAppendBody: Request not yet fully sent \"%s %s\"\n",
	    orig_request->method->string,
	    storeUrl(entry));
	keep_alive = 0;
    }
    /*
     * What does the reply have to say about keep-alive?
     */
    if (!entry->mem_obj->reply->keep_alive)
	keep_alive = 0;
    if (keep_alive) {
	int pinned = 0;
	if (orig_request->flags.tproxy) {
	    client_addr = &httpState->request->client_addr;
	}
	/* yes we have to clear all these! */
	commSetDefer(fd, NULL, NULL);
	commSetTimeout(fd, -1, NULL, NULL);
	commSetSelect(fd, COMM_SELECT_READ, NULL, NULL, 0);
#if DELAY_POOLS
	delayClearNoDelay(fd);
#endif
	comm_remove_close_handler(fd, httpStateFree, httpState);
	fwdUnregister(fd, httpState->fwd);
	if (request->flags.pinned) {
	    pinned = 1;
	} else if (request->flags.connection_auth && request->flags.auth_sent) {
	    pinned = 1;
	}
	if (orig_request->pinned_connection && pinned) {
	    clientPinConnection(orig_request->pinned_connection, fd, orig_request, httpState->peer, request->flags.connection_auth);
	} else if (httpState->peer) {
	    if (httpState->peer->options.originserver)
		pconnPush(fd, httpState->peer->name, httpState->peer->http_port, httpState->orig_request->host, client_addr, client_port);
	    else
		pconnPush(fd, httpState->peer->name, httpState->peer->http_port, "*", client_addr, client_port);
	} else {
	    pconnPush(fd, request->host, request->port, NULL, client_addr, client_port);
	}
	fwdComplete(httpState->fwd);
	httpState->fd = -1;
	httpStateFree(fd, httpState);
    } else {
	fwdComplete(httpState->fwd);
	comm_close(fd);
    }
}

/* This will be called when data is ready to be read from fd.  Read until
 * error or connection closed. */

/* THIS IS THE NEW ONE - completely untested, not used by default -adrian */
static void
httpReadReply(int fd, void *data)
{
    HttpStateData *httpState = data;
    StoreEntry *entry = httpState->entry;
    ssize_t len;
    int bin;
    int clen;
    int done = 0;
    int already_parsed = 0;
    size_t read_sz = SQUID_TCP_SO_RCVBUF;
    int po = 0;
#if DELAY_POOLS
    delay_id delay_id;
#endif
    int buffer_filled;

    if (EBIT_TEST(entry->flags, ENTRY_ABORTED)) {
	comm_close(fd);
	return;
    }
#if DELAY_POOLS
    /* special "if" only for http (for nodelay proxy conns) */
    if (delayIsNoDelay(fd))
	delay_id = 0;
    else
	delay_id = delayMostBytesAllowed(entry->mem_obj, &read_sz);
#endif

    errno = 0;
    CommStats.syscalls.sock.reads++;

    if (memBufIsNull(&httpState->reply_hdr))
	memBufInit(&httpState->reply_hdr, SQUID_TCP_SO_RCVBUF, SQUID_TCP_SO_RCVBUF * 16);

    len = memBufFill(&httpState->reply_hdr, fd, read_sz);
    buffer_filled = len == read_sz;
    debug(11, 5) ("httpReadReply: FD %d: len %d.\n", fd, (int) len);

    /* Len > 0? Account for data; here's where data would be appended to the reply buffer */
    if (len > 0) {
	fd_bytes(fd, len, FD_READ);
#if DELAY_POOLS
	delayBytesIn(delay_id, len);
#endif
	kb_incr(&statCounter.server.all.kbytes_in, len);
	kb_incr(&statCounter.server.http.kbytes_in, len);
	IOStats.Http.reads++;
	for (clen = len - 1, bin = 0; clen; bin++)
	    clen >>= 1;
	IOStats.Http.read_hist[bin]++;
    }

    /* Read size is 0 (EOF); we've not seen any data from the object; its a zero sized reply */
    if (len == 0 && entry->mem_obj->inmem_hi == 0 && !httpState->reply_hdr.size) {
	fwdFail(httpState->fwd, errorCon(ERR_ZERO_SIZE_OBJECT, HTTP_BAD_GATEWAY, httpState->fwd->request));
	httpState->eof = 1;
	comm_close(fd);
	return;
    }
    /* Read size is 0 (EOF); we've not seen any data from the object; its a zero sized reply */
    else if (len == 0) {
	/* Connection closed; retrieval done. */
	httpState->eof = 1;
	if (httpState->reply_hdr_state < 2) {
	    /*
	     * Yes Henrik, there is a point to doing this.  When we
	     * called httpProcessReplyHeader() before, we didn't find
	     * the end of headers, but now we are definately at EOF, so
	     * we want to process the reply headers.
	     */
	    /* Fake an "end-of-headers" to work around such broken servers */
	    httpAppendReplyHeader(httpState, "\r\n", 2);
	}
    }

    /* Len < 0? Error */
    if (len < 0) {
	debug(11, 2) ("httpReadReply: FD %d: read failure: %s.\n",
	    fd, xstrerror());
	if (ignoreErrno(errno)) {
	    commSetSelect(fd, COMM_SELECT_READ, httpReadReply, httpState, 0);
	} else {
	    ErrorState *err;
	    err = errorCon(ERR_READ_ERROR, HTTP_BAD_GATEWAY, httpState->fwd->request);
	    err->xerrno = errno;
	    fwdFail(httpState->fwd, err);
	    comm_close(fd);
	}
	return;
    }

    /* This will be replaced with some logic to only append and parse a data + offset */
    /* Trim whitespace from the incoming buffer */
    if (!httpState->reply_hdr.size && len > 0 && fd_table[fd].uses > 1) {
	/* Skip whitespace */
	while (po < httpState->reply_hdr.size && xisspace(httpState->reply_hdr.buf[po])) {
	    po++;
	}
	if (po == httpState->reply_hdr.size) {
	    /* Continue to read... */
	    /* Timeout NOT increased. This whitespace was from previous reply */
	    commSetSelect(fd, COMM_SELECT_READ, httpReadReply, httpState, 0);
	    return;
	}
    }

    /*
     * At this point; len > 0; there's no error; and we've put the data into the incoming
     * buffer. Lets now try parsing the reply buffer as it stands.
     */

    /* Has the response been parsed? If not, buffer the StoreEntry and then try parsing it */

    if (httpState->reply_hdr_state == 2)
	already_parsed = 1;	/* ie, we've already parsed this reply, no need to repeat stuff */
    
    /* XXX Handle 1xx response skipping here - ugly! */
    /*
     * XXX its unfortunate that we'll be parsing all 1xx responses in the buffer each read()
     * XXX until we see the first non-1xx response; will have to put up with that for now!
     */
    while (httpState->reply_hdr_state < 2 && po < httpState->reply_hdr.size) {
        HttpReply *reply = NULL;

	/* Try parsing */
	storeBuffer(entry);
        done = httpProcessReplyHeader(httpState, po);
        reply = entry->mem_obj->reply;

	/* Did we get a successful parse but 1xx? Try again */
        if (reply->sline.status >= 100 && reply->sline.status < 200) {
		debug(1, 1) ("httpReadReply: FD %d: skipping 1xx response!\n", fd);
		httpReplyReset(reply);
		httpState->reply_hdr_state = 0;
		po += done;		/* Skip the reply in the incoming buffer */
		done = 0;		/* So we don't double-account */
	} else {
		/* Fail or not in parsing - we only do this loop once; and handle errors later */
		break;
	}
    }

    /* Is the header too large? Error out */
    if (httpState->reply_hdr_state == 2 && entry->mem_obj->reply->sline.status == HTTP_HEADER_TOO_LARGE) {
	    storeEntryReset(entry);
	    fwdFail(httpState->fwd, errorCon(ERR_TOO_BIG, HTTP_BAD_GATEWAY, httpState->fwd->request));
	    httpState->fwd->flags.dont_retry = 1;
	    comm_close(fd);
	    return;
    }
    /* Also explicitly handle "reply is too large" here. */
    /*
     * This is important to prevent a DoS because in the old way, data would be moved around
     * and the incoming buffer(s) wouldn't grow in case of whitespace and skipped 1xx replies.
     * Since this isn't the case (in preparation for reference counted buffers), we need
     * to be extra careful the other end isn't sending us too much.
     */

    /*
     * XXX this may end up generating errors for previously working sites, if they somehow
     * XXX send large enough whitespace and/or 1xx responses to tickle maxReplyHeaderSize.
     * XXX this should really be dealt with somewhere else!
     */

    /*
     * We should've handled parsing a reply by this point - if we haven't, and the incoming
     * buffer is still too large, then error out.
     */
    if (httpState->reply_hdr_state < 2 && httpState->reply_hdr.size > Config.maxReplyHeaderSize) {
	    storeEntryReset(entry);
	    fwdFail(httpState->fwd, errorCon(ERR_TOO_BIG, HTTP_BAD_GATEWAY, httpState->fwd->request));
	    httpState->fwd->flags.dont_retry = 1;
	    comm_close(fd);
	    return;
    }

    /* Waiting for more data? Re-register for read and finish */
    if (httpState->reply_hdr_state < 2) {
	commSetTimeout(fd, Config.Timeout.read, NULL, NULL);
	commSetSelect(fd, COMM_SELECT_READ, httpReadReply, httpState, 0);
	return;
    }

    /* Invalid header, not HTTP/0.9? Error out */
    if (httpState->reply_hdr_state == 2 && entry->mem_obj->reply->sline.status == HTTP_INVALID_HEADER &&
	!(entry->mem_obj->reply->sline.version.major == 0 && entry->mem_obj->reply->sline.version.minor == 9)) {
	    storeEntryReset(entry);
	    fwdFail(httpState->fwd, errorCon(ERR_INVALID_RESP, HTTP_BAD_GATEWAY, httpState->fwd->request));
            httpState->fwd->flags.dont_retry = 1;
	    comm_close(fd);
	    return;
    }

    /* Is it a HTTP/0.9 reply? Handle it */
    if (! already_parsed && httpState->reply_hdr_state == 2 && entry->mem_obj->reply->sline.status == HTTP_INVALID_HEADER
	&& (entry->mem_obj->reply->sline.version.major == 0 && entry->mem_obj->reply->sline.version.minor == 9)) {
        /* This bit handles the HTTP/0.9 reply magic */
	    /* The question is, what happens to the data thats ignored in the call to httpProcessReplyHeader()
	     * during the HTTP/0.9 "reply.size = old_size" call? :) */
	    MemBuf mb;
	    HttpReply *reply = entry->mem_obj->reply;
	    httpBuildVersion(&reply->sline.version, 0, 9);
	    reply->sline.status = HTTP_OK;
	    httpHeaderPutTime(&reply->header, HDR_DATE, squid_curtime);
	    mb = httpReplyPack(reply);
	    /* Append the packed "faked" reply status+headers */
	    storeAppend(entry, mb.buf, mb.size);
	    /* Append the reply buffer - that is now just "body" */
	    storeAppend(entry, httpState->reply_hdr.buf + po, httpState->reply_hdr.size - po);
	    memBufClean(&httpState->reply_hdr);
	    httpReplyReset(reply);
	    /* Parse the "faked" headers as the actual reply status+headers as the actual reply */
	    httpReplyParse(reply, mb.buf, mb.size);
	    memBufClean(&mb);
    }

    /* Is it just a straight EOF? Append the EOF.
     * XXX Does the socket need to be closed here? Good question. What did the old code do? */
    if (! already_parsed && httpState->reply_hdr_state == 2 && httpState->eof == 1) {
	httpAppendBody(httpState, NULL, 0, -1);	/* EOF */
	return;
    }

    if (already_parsed && httpState->reply_hdr_state == 2) {
#if WIP_FWD_LOG
	fwdStatus(httpState->fwd, s);
#endif
	/*
	 * If its not a reply that we will re-forward, then
	 * allow the client to get it.
	 */
	if (!fwdReforwardableStatus(entry->mem_obj->reply->sline.status))
	    EBIT_CLR(entry->flags, ENTRY_FWD_HDR_WAIT);
    }

    /* Ok. Its been parsed if it needs to be, all other error conditions handled. */
    assert(httpState->reply_hdr_state == 2);
    /* lock the httpState; it may be freed by a call to httpAppendBody */
    cbdataLock(httpState);
    httpAppendBody(httpState, httpState->reply_hdr.buf + po + done, httpState->reply_hdr.size - po - done, buffer_filled);
    if (cbdataValid(httpState))
	memBufReset(&httpState->reply_hdr);
    cbdataUnlock(httpState);
    /* httpState may be cleared here */

    /* httpAppendBody() reschedules for IO if required */
    return;
}

/* This will be called when request write is complete. Schedule read of
 * reply. */
static void
httpSendComplete(int fd, char *bufnotused, size_t size, int errflag, void *data)
{
    HttpStateData *httpState = data;
    StoreEntry *entry = httpState->entry;
    debug(11, 5) ("httpSendComplete: FD %d: size %d: errflag %d.\n",
	fd, (int) size, errflag);
#if URL_CHECKSUM_DEBUG
    assert(entry->mem_obj->chksum == url_checksum(entry->mem_obj->url));
#endif
    if (size > 0) {
	fd_bytes(fd, size, FD_WRITE);
	kb_incr(&statCounter.server.all.kbytes_out, size);
	kb_incr(&statCounter.server.http.kbytes_out, size);
    }
    if (errflag == COMM_ERR_CLOSING)
	return;
    if (errflag) {
	ErrorState *err;
	err = errorCon(ERR_WRITE_ERROR, HTTP_BAD_GATEWAY, httpState->fwd->request);
	err->xerrno = errno;
	fwdFail(httpState->fwd, err);
	comm_close(fd);
	return;
    } else {
	/*
	 * Set the read timeout here because it hasn't been set yet.
	 * We only set the read timeout after the request has been
	 * fully written to the server-side.  If we start the timeout
	 * after connection establishment, then we are likely to hit
	 * the timeout for POST/PUT requests that have very large
	 * request bodies.
	 */
	commSetTimeout(fd, Config.Timeout.read, httpTimeout, httpState);
	commSetDefer(fd, fwdCheckDeferRead, entry);
    }
    httpState->flags.request_sent = 1;
}

/*
 * build request headers and append them to a given MemBuf 
 * used by httpBuildRequestPrefix()
 * note: calls httpHeaderInit(), the caller is responsible for Clean()-ing
 */
void
httpBuildRequestHeader(request_t * request,
    request_t * orig_request,
    StoreEntry * entry,
    HttpHeader * hdr_out,
    http_state_flags flags)
{
    /* building buffer for complex strings */
#define BBUF_SZ (MAX_URL+32)
    LOCAL_ARRAY(char, bbuf, BBUF_SZ);
    String strConnection = StringNull;
    const HttpHeader *hdr_in = &orig_request->header;
    int we_do_ranges;
    const HttpHeaderEntry *e;
    String strFwd;
    HttpHeaderPos pos = HttpHeaderInitPos;
    String etags = StringNull;

    httpHeaderInit(hdr_out, hoRequest);
    /* append our IMS header */
    if (request->lastmod > -1)
	httpHeaderPutTime(hdr_out, HDR_IF_MODIFIED_SINCE, request->lastmod);
    if (request->etag) {
	etags = httpHeaderGetList(hdr_in, HDR_IF_NONE_MATCH);
	strListAddUnique(&etags, request->etag, ',');
    } else if (request->etags) {
	int i;
	etags = httpHeaderGetList(hdr_in, HDR_IF_NONE_MATCH);
	for (i = 0; i < request->etags->count; i++)
	    strListAddUnique(&etags, request->etags->items[i], ',');
    }
    if (strLen(etags))
	httpHeaderPutString(hdr_out, HDR_IF_NONE_MATCH, &etags);
    stringClean(&etags);
    /* decide if we want to do Ranges ourselves 
     * (and fetch the whole object now)
     * We want to handle Ranges ourselves iff
     *    - we can actually parse client Range specs
     *    - the specs are expected to be simple enough (e.g. no out-of-order ranges)
     *    - reply will be cachable
     * (If the reply will be uncachable we have to throw it away after 
     *  serving this request, so it is better to forward ranges to 
     *  the server and fetch only the requested content) 
     */
    if (NULL == orig_request->range)
	we_do_ranges = 0;
    else if (!orig_request->flags.cachable)
	we_do_ranges = 0;
    else if (orig_request->flags.auth)
	we_do_ranges = 0;
    else if (httpHdrRangeOffsetLimit(orig_request->range))
	we_do_ranges = 0;
    else
	we_do_ranges = 1;
    debug(11, 8) ("httpBuildRequestHeader: range specs: %p, cachable: %d; we_do_ranges: %d\n",
	orig_request->range, orig_request->flags.cachable, we_do_ranges);

    strConnection = httpHeaderGetList(hdr_in, HDR_CONNECTION);
    while ((e = httpHeaderGetEntry(hdr_in, &pos))) {
	debug(11, 5) ("httpBuildRequestHeader: %.*s: %.*s\n",
	    strLen2(e->name), strBuf2(e->name), strLen2(e->value), strBuf2(e->value));
	if (!httpRequestHdrAllowed(e, &strConnection)) {
	    debug(11, 2) ("'%.*s' header is a hop-by-hop connections header\n",
		strLen2(e->name), strBuf2(e->name));
	    continue;
	}
	switch (e->id) {
	case HDR_PROXY_AUTHORIZATION:
	    /* Only pass on proxy authentication to peers for which
	     * authentication forwarding is explicitly enabled
	     */
	    if (flags.proxying && orig_request->peer_login && strcmp(orig_request->peer_login, "PASS") == 0) {
		httpHeaderAddClone(hdr_out, e);
		if (request->flags.connection_proxy_auth)
		    request->flags.pinned = 1;
	    }
	    break;
	case HDR_AUTHORIZATION:
	    /* Pass on WWW authentication.
	     */
	    if (!flags.originpeer) {
		httpHeaderAddClone(hdr_out, e);
		if (orig_request->flags.connection_auth)
		    orig_request->flags.pinned = 1;
	    } else {
		/* In accelerators, only forward authentication if enabled
		 * (see also below for proxy->server authentication)
		 */
		if (orig_request->peer_login && (strcmp(orig_request->peer_login, "PASS") == 0 || strcmp(orig_request->peer_login, "PROXYPASS") == 0)) {
		    httpHeaderAddClone(hdr_out, e);
		    if (orig_request->flags.connection_auth)
			orig_request->flags.pinned = 1;
		}
	    }
	    break;
	case HDR_HOST:
	    /*
	     * Normally Squid rewrites the Host: header.
	     * However, there is one case when we don't: If the URL
	     * went through our redirector and the admin configured
	     * 'redir_rewrites_host' to be off.
	     */
	    if (orig_request->peer_domain)
		httpHeaderPutStr(hdr_out, HDR_HOST, orig_request->peer_domain);
	    else if (request->flags.redirected && !Config.onoff.redir_rewrites_host)
		httpHeaderAddClone(hdr_out, e);
	    else {
		/* use port# only if not default */
		if (orig_request->port == urlDefaultPort(orig_request->protocol)) {
		    httpHeaderPutStr(hdr_out, HDR_HOST, orig_request->host);
		} else {
		    httpHeaderPutStrf(hdr_out, HDR_HOST, "%s:%d",
			orig_request->host, (int) orig_request->port);
		}
	    }
	    break;
	case HDR_IF_MODIFIED_SINCE:
	    /* append unless we added our own;
	     * note: at most one client's ims header can pass through */
	    if (!httpHeaderHas(hdr_out, HDR_IF_MODIFIED_SINCE))
		if (!Config.onoff.ignore_ims_on_miss || !orig_request->flags.cachable || orig_request->flags.auth)
		    httpHeaderAddClone(hdr_out, e);
	    break;
	case HDR_IF_NONE_MATCH:
	    /* append unless ignore_ims_on_miss is in effect */
	    if (!httpHeaderHas(hdr_out, HDR_IF_NONE_MATCH))
		if (!Config.onoff.ignore_ims_on_miss || !orig_request->flags.cachable || orig_request->flags.auth)
		    httpHeaderAddClone(hdr_out, e);
	    break;
	case HDR_MAX_FORWARDS:
	    if (orig_request->method->code == METHOD_TRACE) {
		/* sacrificing efficiency over clarity, etc. */
		const int hops = httpHeaderGetInt(hdr_in, HDR_MAX_FORWARDS);
		if (hops > 0)
		    httpHeaderPutInt(hdr_out, HDR_MAX_FORWARDS, hops - 1);
	    }
	    break;
	case HDR_X_FORWARDED_FOR:
	    if (opt_forwarded_for == FORWARDED_FOR_TRANSPARENT)
		httpHeaderAddClone(hdr_out, e);
	    break;
	case HDR_RANGE:
	case HDR_IF_RANGE:
	case HDR_REQUEST_RANGE:
	    if (!we_do_ranges)
		httpHeaderAddClone(hdr_out, e);
	    break;
	case HDR_VIA:
	    /* If Via is disabled then forward any received header as-is */
	    if (!Config.onoff.via)
		httpHeaderAddClone(hdr_out, e);
	    break;
	case HDR_CONNECTION:
	case HDR_KEEP_ALIVE:
	    /* case HDR_PROXY_AUTHORIZATION: is special and handled above */
	case HDR_PROXY_AUTHENTICATE:
	case HDR_TE:
	case HDR_TRAILER:
	case HDR_TRANSFER_ENCODING:
	case HDR_UPGRADE:
	case HDR_PROXY_CONNECTION:
	    /* hop-by-hop headers. Don't forward */
	    break;
	case HDR_CACHE_CONTROL:
	    /* append these after the loop if needed */
	    break;
	case HDR_FRONT_END_HTTPS:
	    if (!flags.front_end_https)
		httpHeaderAddClone(hdr_out, e);
	    break;
	default:
	    /* pass on all other header fields */
	    httpHeaderAddClone(hdr_out, e);
	}
    }

    /* append Via */
    if (Config.onoff.via) {
	String strVia = httpHeaderGetList(hdr_in, HDR_VIA);
	snprintf(bbuf, BBUF_SZ, "%d.%d %s",
	    orig_request->http_ver.major,
	    orig_request->http_ver.minor, ThisCache);
	strListAdd(&strVia, bbuf, ',');
	if (flags.http11)
	    strListAdd(&strVia, "1.0 internal", ',');
	httpHeaderPutString(hdr_out, HDR_VIA, &strVia);
	stringClean(&strVia);
    }
    /* append X-Forwarded-For */
    strFwd = StringNull;
    switch (opt_forwarded_for) {
    case FORWARDED_FOR_ON:
    case FORWARDED_FOR_OFF:
	strFwd = httpHeaderGetList(hdr_in, HDR_X_FORWARDED_FOR);
    case FORWARDED_FOR_TRUNCATE:
	strListAdd(&strFwd, (((orig_request->client_addr.s_addr != no_addr.s_addr) && opt_forwarded_for != FORWARDED_FOR_OFF) ?
		inet_ntoa(orig_request->client_addr) : "unknown"), ',');
	break;
    case FORWARDED_FOR_TRANSPARENT:
	/* Handled above */
	break;
    case FORWARDED_FOR_DELETE:
	/* Nothing to do */
	break;
    }
    if (strLen(strFwd)) {
	httpHeaderPutString(hdr_out, HDR_X_FORWARDED_FOR, &strFwd);
	stringClean(&strFwd);
    }
    /* append Host if not there already */
    if (!httpHeaderHas(hdr_out, HDR_HOST)) {
	if (orig_request->peer_domain) {
	    httpHeaderPutStr(hdr_out, HDR_HOST, orig_request->peer_domain);
	} else if (orig_request->port == urlDefaultPort(orig_request->protocol)) {
	    /* use port# only if not default */
	    httpHeaderPutStr(hdr_out, HDR_HOST, orig_request->host);
	} else {
	    httpHeaderPutStrf(hdr_out, HDR_HOST, "%s:%d",
		orig_request->host, (int) orig_request->port);
	}
    }
    /* append Authorization if known in URL, not in header and going direct */
    if (!httpHeaderHas(hdr_out, HDR_AUTHORIZATION)) {
	if (!request->flags.proxying) {
	    if (*request->login) {
		httpHeaderPutStrf(hdr_out, HDR_AUTHORIZATION, "Basic %s",
		    base64_encode(request->login));
	    } else if (orig_request->extacl_user && orig_request->extacl_passwd) {
		char loginbuf[256];
		snprintf(loginbuf, sizeof(loginbuf), "%s:%s", orig_request->extacl_user, orig_request->extacl_passwd);
		httpHeaderPutStrf(hdr_out, HDR_AUTHORIZATION, "Basic %s",
		    base64_encode(loginbuf));
	    }
	}
    }
    /* append Proxy-Authorization if configured for peer, and proxying */
    if (request->flags.proxying && orig_request->peer_login &&
	!httpHeaderHas(hdr_out, HDR_PROXY_AUTHORIZATION)) {
	if (*orig_request->peer_login == '*') {
	    /* Special mode, to pass the username to the upstream cache */
	    char loginbuf[256];
	    const char *username = "-";
	    if (orig_request->extacl_user)
		username = orig_request->extacl_user;
	    else if (orig_request->auth_user_request)
		username = authenticateUserRequestUsername(orig_request->auth_user_request);
	    snprintf(loginbuf, sizeof(loginbuf), "%s%s", username, orig_request->peer_login + 1);
	    httpHeaderPutStrf(hdr_out, HDR_PROXY_AUTHORIZATION, "Basic %s",
		base64_encode(loginbuf));
	} else if (strcmp(orig_request->peer_login, "PASS") == 0) {
	    if (orig_request->extacl_user && orig_request->extacl_passwd) {
		char loginbuf[256];
		snprintf(loginbuf, sizeof(loginbuf), "%s:%s", orig_request->extacl_user, orig_request->extacl_passwd);
		httpHeaderPutStrf(hdr_out, HDR_PROXY_AUTHORIZATION, "Basic %s",
		    base64_encode(loginbuf));
	    }
	} else if (strcmp(orig_request->peer_login, "PROXYPASS") == 0) {
	    /* Nothing to do */
	} else {
	    httpHeaderPutStrf(hdr_out, HDR_PROXY_AUTHORIZATION, "Basic %s",
		base64_encode(orig_request->peer_login));
	}
    }
    /* append WWW-Authorization if configured for peer */
    if (flags.originpeer && orig_request->peer_login &&
	!httpHeaderHas(hdr_out, HDR_AUTHORIZATION)) {
	if (strcmp(orig_request->peer_login, "PASS") == 0) {
	    /* No credentials to forward.. (should have been done above if available) */
	} else if (strcmp(orig_request->peer_login, "PROXYPASS") == 0) {
	    /* Special mode, convert proxy authentication to WWW authentication
	     * (also applies to cookie authentication)
	     */
	    const char *auth = httpHeaderGetStr(hdr_in, HDR_PROXY_AUTHORIZATION);
	    if (auth && strncasecmp(auth, "basic ", 6) == 0) {
		httpHeaderPutStr(hdr_out, HDR_AUTHORIZATION, auth);
		if (orig_request->flags.connection_auth)
		    orig_request->flags.pinned = 1;
	    } else if (orig_request->extacl_user && orig_request->extacl_passwd) {
		char loginbuf[256];
		snprintf(loginbuf, sizeof(loginbuf), "%s:%s", orig_request->extacl_user, orig_request->extacl_passwd);
		httpHeaderPutStrf(hdr_out, HDR_AUTHORIZATION, "Basic %s",
		    base64_encode(loginbuf));
	    }
	} else if (*orig_request->peer_login == '*') {
	    /* Special mode, to pass the username to the upstream cache */
	    char loginbuf[256];
	    const char *username = "-";
	    if (orig_request->auth_user_request)
		username = authenticateUserRequestUsername(orig_request->auth_user_request);
	    else if (orig_request->extacl_user)
		username = orig_request->extacl_user;
	    snprintf(loginbuf, sizeof(loginbuf), "%s%s", username, orig_request->peer_login + 1);
	    httpHeaderPutStrf(hdr_out, HDR_AUTHORIZATION, "Basic %s",
		base64_encode(loginbuf));
	} else {
	    /* Fixed login string */
	    httpHeaderPutStrf(hdr_out, HDR_AUTHORIZATION, "Basic %s",
		base64_encode(orig_request->peer_login));
	}
    }
    /* append Cache-Control, add max-age if not there already */
    {
	HttpHdrCc *cc = httpHeaderGetCc(hdr_in);
	if (!cc)
	    cc = httpHdrCcCreate();
	if (!EBIT_TEST(cc->mask, CC_MAX_AGE)) {
	    const char *url = entry ? storeUrl(entry) : urlCanonical(orig_request);
	    httpHdrCcSetMaxAge(cc, getMaxAge(url));
	    if (strLen(request->urlpath))
		assert(strstr(url, strBuf(request->urlpath)));
	}
	/* Set no-cache if determined needed but not found */
	if (orig_request->flags.nocache && !httpHeaderHas(hdr_in, HDR_PRAGMA))
	    EBIT_SET(cc->mask, CC_NO_CACHE);
	/* Enforce sibling relations */
	if (flags.only_if_cached)
	    EBIT_SET(cc->mask, CC_ONLY_IF_CACHED);
	httpHeaderPutCc(hdr_out, cc);
	httpHdrCcDestroy(cc);
    }
    /* maybe append Connection: keep-alive */
    if (flags.keepalive || request->flags.pinned) {
	if (flags.proxying) {
	    httpHeaderPutStr(hdr_out, HDR_PROXY_CONNECTION, "keep-alive");
	} else {
	    httpHeaderPutStr(hdr_out, HDR_CONNECTION, "keep-alive");
	}
    }
    /* append Front-End-Https */
    if (flags.front_end_https) {
	if (flags.front_end_https == 1 || request->protocol == PROTO_HTTPS)
	    httpHeaderPutStr(hdr_out, HDR_FRONT_END_HTTPS, "On");
    }
    /* Now mangle the headers. */
    httpHdrMangleList(hdr_out, orig_request);
    stringClean(&strConnection);
}

/* build request prefix and append it to a given MemBuf; 
 * return the length of the prefix */
int
httpBuildRequestPrefix(request_t * request,
    request_t * orig_request,
    StoreEntry * entry,
    MemBuf * mb,
    http_state_flags flags)
{
    const int offset = mb->size;
    memBufPrintf(mb, "%s %.*s HTTP/1.%d\r\n",
        request->method->string,
	strLen2(request->urlpath) ? strLen2(request->urlpath) : 1,
	strLen2(request->urlpath) ? strBuf2(request->urlpath) : "/",
	flags.http11);
    /* build and pack headers */
    {
	HttpHeader hdr;
	Packer p;
	httpBuildRequestHeader(request, orig_request, entry, &hdr, flags);
	if (request->flags.pinned && request->flags.connection_auth)
	    request->flags.auth_sent = 1;
	else
	    request->flags.auth_sent = httpHeaderHas(&hdr, HDR_AUTHORIZATION);
	packerToMemInit(&p, mb);
	httpHeaderPackInto(&hdr, &p);
	httpHeaderClean(&hdr);
	packerClean(&p);
    }
    /* append header terminator */
    memBufAppend(mb, crlf, 2);
    return mb->size - offset;
}

/* This will be called when connect completes. Write request. */
static void
httpSendRequest(HttpStateData * httpState)
{
    MemBuf mb;
    request_t *req = httpState->request;
    StoreEntry *entry = httpState->entry;
    peer *p = httpState->peer;
    CWCB *sendHeaderDone;
    int fd = httpState->fd;

    debug(11, 5) ("httpSendRequest: FD %d: httpState %p.\n", fd, httpState);

    /* Schedule read reply. (but no timeout set until request fully sent) */
    commSetTimeout(fd, Config.Timeout.lifetime, httpTimeout, httpState);
    commSetSelect(fd, COMM_SELECT_READ, httpReadReply, httpState, 0);

    if (httpState->orig_request->body_reader)
	sendHeaderDone = httpSendRequestEntry;
    else
	sendHeaderDone = httpSendComplete;

    if (p != NULL) {
	if (p->options.originserver)
	    httpState->flags.originpeer = 1;
	else
	    httpState->flags.proxying = 1;
    } else {
	httpState->flags.proxying = 0;
	httpState->flags.originpeer = 0;
    }
    /*
     * Is keep-alive okay for all request methods?
     */
    if (!Config.onoff.server_pconns)
	httpState->flags.keepalive = 0;
    else if (p == NULL)
	httpState->flags.keepalive = 1;
    else if (p->stats.n_keepalives_sent < 10)
	httpState->flags.keepalive = 1;
    else if ((double) p->stats.n_keepalives_recv / (double) p->stats.n_keepalives_sent > 0.50)
	httpState->flags.keepalive = 1;
    if (httpState->peer) {
	if (neighborType(httpState->peer, httpState->request) == PEER_SIBLING &&
	    !httpState->peer->options.allow_miss)
	    httpState->flags.only_if_cached = 1;
	httpState->flags.front_end_https = httpState->peer->front_end_https;
    }
    if (httpState->peer)
	httpState->flags.http11 = httpState->peer->options.http11;
    else
	httpState->flags.http11 = Config.onoff.server_http11;
    memBufDefInit(&mb);
    httpBuildRequestPrefix(req,
	httpState->orig_request,
	entry,
	&mb,
	httpState->flags);
    if (req->flags.pinned)
	httpState->flags.keepalive = 1;
    debug(11, 6) ("httpSendRequest: FD %d:\n%s\n", fd, mb.buf);
    comm_write_mbuf(fd, mb, sendHeaderDone, httpState);
}

CBDATA_TYPE(HttpStateData);

void
httpStart(FwdState * fwd)
{
    int fd = fwd->server_fd;
    HttpStateData *httpState;
    request_t *proxy_req;
    request_t *orig_req = fwd->request;
    debug(11, 3) ("httpStart: \"%s %s\"\n", orig_req->method->string,
	storeUrl(fwd->entry));
    CBDATA_INIT_TYPE(HttpStateData);
    httpState = cbdataAlloc(HttpStateData);
    storeLockObject(fwd->entry);
    httpState->fwd = fwd;
    httpState->entry = fwd->entry;
    httpState->fd = fd;
    if (fwd->servers)
	httpState->peer = fwd->servers->peer;	/* might be NULL */
    if (httpState->peer) {
	const char *url;
	if (httpState->peer->options.originserver)
	    url = strBuf(orig_req->urlpath);
	else
	    url = storeUrl(httpState->entry);
	proxy_req = requestCreate(orig_req->method,
	    orig_req->protocol, url);
	xstrncpy(proxy_req->host, httpState->peer->host, SQUIDHOSTNAMELEN);
	proxy_req->port = httpState->peer->http_port;
	proxy_req->flags = orig_req->flags;
	proxy_req->lastmod = orig_req->lastmod;
	httpState->request = requestLink(proxy_req);
	httpState->orig_request = requestLink(orig_req);
	proxy_req->flags.proxying = 1;
	/*
	 * This NEIGHBOR_PROXY_ONLY check probably shouldn't be here.
	 * We might end up getting the object from somewhere else if,
	 * for example, the request to this neighbor fails.
	 */
	if (httpState->peer->options.proxy_only)
	    storeReleaseRequest(httpState->entry);
#if DELAY_POOLS
	assert(delayIsNoDelay(fd) == 0);
	if (httpState->peer->options.no_delay)
	    delaySetNoDelay(fd);
#endif
    } else {
	httpState->request = requestLink(orig_req);
	httpState->orig_request = requestLink(orig_req);
    }
    /*
     * register the handler to free HTTP state data when the FD closes
     */
    comm_add_close_handler(fd, httpStateFree, httpState);
    statCounter.server.all.requests++;
    statCounter.server.http.requests++;
    httpSendRequest(httpState);
    /*
     * We used to set the read timeout here, but not any more.
     * Now its set in httpSendComplete() after the full request,
     * including request body, has been written to the server.
     */
}

static void
httpSendRequestEntryDone(int fd, void *data)
{
    HttpStateData *httpState = data;
    debug(11, 5) ("httpSendRequestEntryDone: FD %d\n",
	fd);
    if (!Config.accessList.brokenPosts) {
	debug(11, 5) ("httpSendRequestEntryDone: No brokenPosts list\n");
	httpSendComplete(fd, NULL, 0, 0, data);
    } else if (!aclCheckFastRequest(Config.accessList.brokenPosts, httpState->request)) {
	debug(11, 5) ("httpSendRequestEntryDone: didn't match brokenPosts\n");
	httpSendComplete(fd, NULL, 0, 0, data);
    } else {
	debug(11, 2) ("httpSendRequestEntryDone: matched brokenPosts\n");
	comm_write(fd, "\r\n", 2, httpSendComplete, data, NULL);
    }
}

static void
httpRequestBodyHandler2(void *data)
{
    HttpStateData *httpState = (HttpStateData *) data;
    char *buf = httpState->body_buf;
    httpState->body_buf = NULL;
    comm_write(httpState->fd, buf, httpState->body_buf_sz, httpSendRequestEntry, data, memFree8K);
}

static void
httpRequestBodyHandler(char *buf, ssize_t size, void *data)
{
    HttpStateData *httpState = (HttpStateData *) data;
    httpState->body_buf = NULL;
    if (size > 0) {
	if (httpState->reply_hdr_state >= 2 && !httpState->flags.abuse_detected) {
	    httpState->flags.abuse_detected = 1;
	    debug(11, 1) ("httpRequestBodyHandler: Likely proxy abuse detected '%s' -> '%s'\n",
		inet_ntoa(httpState->orig_request->client_addr),
		storeUrl(httpState->entry));
	    if (httpState->entry->mem_obj->reply->sline.status == HTTP_INVALID_HEADER) {
		memFree8K(buf);
		comm_close(httpState->fd);
		return;
	    }
	    httpState->body_buf = buf;
	    httpState->body_buf_sz = size;
	    /* Give response some time to propagate before sending rest
	     * of request in case of error */
	    eventAdd("POST delay on response", httpRequestBodyHandler2, httpState, 2.0, 1);
	    return;
	}
	comm_write(httpState->fd, buf, size, httpSendRequestEntry, data, memFree8K);
    } else if (size == 0) {
	/* End of body */
	memFree8K(buf);
	httpSendRequestEntryDone(httpState->fd, data);
    } else {
	/* Failed to get whole body, probably aborted */
	memFree8K(buf);
	httpSendComplete(httpState->fd, NULL, 0, COMM_ERR_CLOSING, data);
    }
}

static void
httpSendRequestEntry(int fd, char *bufnotused, size_t size, int errflag, void *data)
{
    HttpStateData *httpState = data;
    StoreEntry *entry = httpState->entry;
    debug(11, 5) ("httpSendRequestEntry: FD %d: size %d: errflag %d.\n",
	fd, (int) size, errflag);
    if (size > 0) {
	fd_bytes(fd, size, FD_WRITE);
	kb_incr(&statCounter.server.all.kbytes_out, size);
	kb_incr(&statCounter.server.http.kbytes_out, size);
    }
    if (errflag == COMM_ERR_CLOSING)
	return;
    if (errflag) {
	ErrorState *err;
	err = errorCon(ERR_WRITE_ERROR, HTTP_BAD_GATEWAY, httpState->fwd->request);
	err->xerrno = errno;
	fwdFail(httpState->fwd, err);
	comm_close(fd);
	return;
    }
    if (EBIT_TEST(entry->flags, ENTRY_ABORTED)) {
	comm_close(fd);
	return;
    }
    httpState->body_buf = memAllocate(MEM_8K_BUF);
    requestReadBody(httpState->orig_request, httpState->body_buf, 8192, httpRequestBodyHandler, httpState);
}

void
httpBuildVersion(http_version_t * version, unsigned int major, unsigned int minor)
{
    version->major = major;
    version->minor = minor;
}


/*
 * $Id$
 *
 * DEBUG: section 33    Client-side Routines
 * AUTHOR: Duane Wessels
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * --------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by
 *  the National Science Foundation.
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  
 */

#include "squid.h"

static const char *const crlf = "\r\n";
static const char *const proxy_auth_challenge =
"Basic realm=\"Squid proxy-caching web server\"";

#define REQUEST_BUF_SIZE 4096
#define FAILURE_MODE_TIME 300

/* Local functions */

static CWCB clientHandleIMSComplete;
static CWCB clientWriteComplete;
static PF clientReadRequest;
static PF connStateFree;
static PF requestTimeout;
static STCB clientGetHeadersForIMS;
static int CheckQuickAbort2(const clientHttpRequest *);
static int clientCheckTransferDone(clientHttpRequest *);
static void CheckQuickAbort(clientHttpRequest *);
static void checkFailureRatio(err_type, hier_code);
static void clientProcessMiss(clientHttpRequest *);
static void clientAppendReplyHeader(char *, const char *, size_t *, size_t);
size_t clientBuildReplyHeader(clientHttpRequest *, char *, size_t, size_t *, char *, size_t);
static clientHttpRequest *parseHttpRequest(ConnStateData *, method_t *, int *, char **, size_t *);
static RH clientRedirectDone;
static STCB clientHandleIMSReply;
static int clientGetsOldEntry(StoreEntry * new, StoreEntry * old, request_t * request);
static int checkAccelOnly(clientHttpRequest *);
static STCB clientSendMoreData;
static STCB clientCacheHit;
static void clientParseRequestHeaders(clientHttpRequest *);
static void clientProcessRequest(clientHttpRequest *);
static void clientProcessExpired(void *data);
static void clientProcessOnlyIfCachedMiss(clientHttpRequest * http);
static HttpReply *clientConstructProxyAuthReply(clientHttpRequest * http);
static int clientCachable(clientHttpRequest * http);
static int clientHierarchical(clientHttpRequest * http);
static int clientCheckContentLength(request_t * r);

static int
checkAccelOnly(clientHttpRequest * http)
{
    /* return TRUE if someone makes a proxy request to us and
     * we are in httpd-accel only mode */
    if (!Config2.Accel.on)
	return 0;
    if (Config.onoff.accel_with_proxy)
	return 0;
    if (http->request->protocol == PROTO_CACHEOBJ)
	return 0;
    if (http->accel)
	return 0;
    return 1;
}

void
clientAccessCheck(void *data)
{
    clientHttpRequest *http = data;
    ConnStateData *conn = http->conn;
    char *browser;
    if (Config.onoff.ident_lookup && conn->ident.state == IDENT_NONE) {
	identStart(-1, conn, clientAccessCheck, http);
	return;
    }
    if (checkAccelOnly(http)) {
	clientAccessCheckDone(0, http);
	return;
    }
    browser = mime_get_header(http->request->headers, "User-Agent");
    http->acl_checklist = aclChecklistCreate(Config.accessList.http,
	http->request,
	conn->peer.sin_addr,
	browser,
	conn->ident.ident);
    aclNBCheck(http->acl_checklist, clientAccessCheckDone, http);
}

static HttpReply *
clientConstructProxyAuthReply(clientHttpRequest * http)
{
    ErrorState *err = errorCon(ERR_CACHE_ACCESS_DENIED, HTTP_PROXY_AUTHENTICATION_REQUIRED);
    HttpReply *rep;
    err->request = requestLink(http->request);
    rep = errorBuildReply(err);
    errorStateFree(err);
    /* add Authenticate header */
    httpHeaderPutStr(&rep->header, HDR_PROXY_AUTHENTICATE, proxy_auth_challenge);
    return rep;
}

StoreEntry *
clientCreateStoreEntry(clientHttpRequest * h, method_t m, int flags)
{
    StoreEntry *e;
    request_t *r;
    /*
     * For erroneous requests, we might not have a h->request,
     * so make a fake one.
     */
    if (h->request == NULL) {
	r = memAllocate(MEM_REQUEST_T);
	r->method = m;
	r->protocol = PROTO_NONE;
	h->request = requestLink(r);
    }
    e = storeCreateEntry(h->uri, h->log_uri, flags, m);
    storeClientListAdd(e, h);
    storeClientCopy(e, 0, 0, 4096, memAllocate(MEM_4K_BUF), clientSendMoreData, h);
    return e;
}


void
clientAccessCheckDone(int answer, void *data)
{
    clientHttpRequest *http = data;
    int page_id = -1;
    ErrorState *err = NULL;
    HttpReply *rep;
    debug(33, 5) ("clientAccessCheckDone: '%s' answer=%d\n", http->uri, answer);
    http->acl_checklist = NULL;
    if (answer == ACCESS_ALLOWED) {
	urlCanonical(http->request, http->uri);
	assert(http->redirect_state == REDIRECT_NONE);
	http->redirect_state = REDIRECT_PENDING;
	redirectStart(http, clientRedirectDone, http);
    } else if (answer == ACCESS_REQ_PROXY_AUTH) {
	http->al.http.code = HTTP_PROXY_AUTHENTICATION_REQUIRED;
	http->log_type = LOG_TCP_DENIED;
	http->entry = clientCreateStoreEntry(http, http->request->method, 0);
	/* create appropreate response */
	rep = clientConstructProxyAuthReply(http);
	httpReplySwapOut(rep, http->entry);
	/* do not need it anymore */
	httpReplyDestroy(rep);
    } else {
	debug(33, 5) ("Access Denied: %s\n", http->uri);
	debug(33, 5) ("AclMatchedName = %s\n",
	    AclMatchedName ? AclMatchedName : "<null>");
	http->log_type = LOG_TCP_DENIED;
	http->entry = clientCreateStoreEntry(http, http->request->method, 0);
	page_id = aclGetDenyInfoPage(&Config.denyInfoList, AclMatchedName);
	/* NOTE: don't use HTTP_UNAUTHORIZED because then the
	 * stupid browser wants us to authenticate */
	err = errorCon(ERR_ACCESS_DENIED, HTTP_FORBIDDEN);
	err->request = requestLink(http->request);
	err->src_addr = http->conn->peer.sin_addr;
	if (page_id > 0)
	    err->page_id = page_id;
	errorAppendEntry(http->entry, err);
    }
}

static void
clientRedirectDone(void *data, char *result)
{
    clientHttpRequest *http = data;
    size_t l;
    request_t *new_request = NULL;
    request_t *old_request = http->request;
    debug(33, 5) ("clientRedirectDone: '%s' result=%s\n", http->uri,
	result ? result : "NULL");
    assert(http->redirect_state == REDIRECT_PENDING);
    http->redirect_state = REDIRECT_DONE;
    if (result)
	new_request = urlParse(old_request->method, result);
    if (new_request) {
	safe_free(http->uri);
	/* need to malloc because the URL returned by the redirector might
	 * not be big enough to append the local domain
	 * -- David Lamkin drl@net-tel.co.uk */
	l = strlen(result) + Config.appendDomainLen + 5;
	http->uri = xcalloc(l, 1);
	xstrncpy(http->uri, result, l);
	new_request->http_ver = old_request->http_ver;
	new_request->headers = xstrdup(old_request->headers);
	new_request->headers_sz = old_request->headers_sz;
	if (old_request->body) {
	    new_request->body = xmalloc(old_request->body_sz);
	    xmemcpy(new_request->body, old_request->body, old_request->body_sz);
	    new_request->body_sz = old_request->body_sz;
	}
	requestUnlink(old_request);
	http->request = requestLink(new_request);
	urlCanonical(http->request, http->uri);
    }
    clientParseRequestHeaders(http);
    fd_note(http->conn->fd, http->uri);
    clientProcessRequest(http);
}

static void
clientProcessExpired(void *data)
{
    clientHttpRequest *http = data;
    char *url = http->uri;
    StoreEntry *entry = NULL;
    debug(33, 3) ("clientProcessExpired: '%s'\n", http->uri);
    EBIT_SET(http->request->flags, REQ_REFRESH);
    http->old_entry = http->entry;
    entry = storeCreateEntry(url,
	http->log_uri,
	http->request->flags,
	http->request->method);
    /* NOTE, don't call storeLockObject(), storeCreateEntry() does it */
    storeClientListAdd(entry, http);
    storeClientListAdd(http->old_entry, http);
    entry->lastmod = http->old_entry->lastmod;
    debug(33, 5) ("clientProcessExpired: setting lmt = %d\n",
	(int) entry->lastmod);
    entry->refcount++;		/* EXPIRED CASE */
    http->entry = entry;
    http->out.offset = 0;
    protoDispatch(http->conn->fd, http->entry, http->request);
    /* Register with storage manager to receive updates when data comes in. */
    if (entry->store_status == STORE_ABORTED)
	debug(33, 0) ("clientProcessExpired: entry->swap_status == STORE_ABORTED\n");
    storeClientCopy(entry,
	http->out.offset,
	http->out.offset,
	4096,
	memAllocate(MEM_4K_BUF),
	clientHandleIMSReply,
	http);
}

static int
clientGetsOldEntry(StoreEntry * new_entry, StoreEntry * old_entry, request_t * request)
{
    /* If the reply is anything but "Not Modified" then
     * we must forward it to the client */
    const http_status status = new_entry->mem_obj->reply->sline.status;
    if (status != 304) {
	debug(33, 5) ("clientGetsOldEntry: NO, reply=%d\n", status);
	return 0;
    }
    /* If the client did not send IMS in the request, then it
     * must get the old object, not this "Not Modified" reply */
    if (!EBIT_TEST(request->flags, REQ_IMS)) {
	debug(33, 5) ("clientGetsOldEntry: YES, no client IMS\n");
	return 1;
    }
    /* If the client IMS time is prior to the entry LASTMOD time we
     * need to send the old object */
    if (modifiedSince(old_entry, request)) {
	debug(33, 5) ("clientGetsOldEntry: YES, modified since %d\n",
	    (int) request->ims);
	return 1;
    }
    debug(33, 5) ("clientGetsOldEntry: NO, new one is fine\n");
    return 0;
}


static void
clientHandleIMSReply(void *data, char *buf, ssize_t size)
{
    clientHttpRequest *http = data;
    StoreEntry *entry = http->entry;
    MemObject *mem = entry->mem_obj;
    const char *url = storeUrl(entry);
    int unlink_request = 0;
    StoreEntry *oldentry;
    debug(33, 3) ("clientHandleIMSReply: %s, %d bytes\n", url, (int) size);
    memFree(MEM_4K_BUF, buf);
    buf = NULL;
    /* unregister this handler */
    if (size < 0 || entry->store_status == STORE_ABORTED) {
	debug(33, 3) ("clientHandleIMSReply: ABORTED '%s'\n", url);
	/* We have an existing entry, but failed to validate it */
	/* Its okay to send the old one anyway */
	http->log_type = LOG_TCP_REFRESH_FAIL_HIT;
	storeUnregister(entry, http);
	storeUnlockObject(entry);
	entry = http->entry = http->old_entry;
	entry->refcount++;
    } else if (mem->reply->sline.status == 0) {
	debug(33, 3) ("clientHandleIMSReply: Incomplete headers for '%s'\n", url);
	if (entry->store_status == STORE_ABORTED)
	    debug(33, 0) ("clientHandleIMSReply: entry->swap_status == STORE_ABORTED\n");
	storeClientCopy(entry,
	    http->out.offset + size,
	    http->out.offset,
	    4096,
	    memAllocate(MEM_4K_BUF),
	    clientHandleIMSReply,
	    http);
	return;
    } else if (clientGetsOldEntry(entry, http->old_entry, http->request)) {
	/* We initiated the IMS request, the client is not expecting
	 * 304, so put the good one back.  First, make sure the old entry
	 * headers have been loaded from disk. */
	oldentry = http->old_entry;
	http->log_type = LOG_TCP_REFRESH_HIT;
	if (oldentry->mem_obj->request == NULL) {
	    oldentry->mem_obj->request = requestLink(mem->request);
	    unlink_request = 1;
	}
	/* Don't memcpy() the whole reply structure here.  For example,
	 * www.thegist.com (Netscape/1.13) returns a content-length for
	 * 304's which seems to be the length of the 304 HEADERS!!! and
	 * not the body they refer to.  */
	httpReplyUpdateOnNotModified(entry->mem_obj->reply, oldentry->mem_obj->reply);
	storeTimestampsSet(oldentry);
	storeUnregister(entry, http);
	storeUnlockObject(entry);
	entry = http->entry = oldentry;
	entry->timestamp = squid_curtime;
	if (unlink_request) {
	    requestUnlink(entry->mem_obj->request);
	    entry->mem_obj->request = NULL;
	}
    } else {
	/* the client can handle this reply, whatever it is */
	http->log_type = LOG_TCP_REFRESH_MISS;
	if (mem->reply->sline.status == 304) {
	    http->old_entry->timestamp = squid_curtime;
	    http->old_entry->refcount++;
	    http->log_type = LOG_TCP_REFRESH_HIT;
	}
	storeUnregister(http->old_entry, http);
	storeUnlockObject(http->old_entry);
    }
    http->old_entry = NULL;	/* done with old_entry */
    /* use clientCacheHit() here as the callback because we might
     * be swapping in from disk, and the file might not really be
     * there */
    if (entry->store_status == STORE_ABORTED) {
	debug(33, 0) ("clientHandleIMSReply: IMS swapin failed on aborted object\n");
	http->log_type = LOG_TCP_SWAPFAIL_MISS;
	clientProcessMiss(http);
	return;
    }
    storeClientCopy(entry,
	http->out.offset,
	http->out.offset,
	4096,
	memAllocate(MEM_4K_BUF),
	clientCacheHit,
	http);
}

int
modifiedSince(StoreEntry * entry, request_t * request)
{
    int object_length;
    MemObject *mem = entry->mem_obj;
    debug(33, 3) ("modifiedSince: '%s'\n", storeUrl(entry));
    if (entry->lastmod < 0)
	return 1;
    /* Find size of the object */
    object_length = mem->reply->content_length;
    if (object_length < 0)
	object_length = contentLen(entry);
    if (entry->lastmod > request->ims) {
	debug(33, 3) ("--> YES: entry newer than client\n");
	return 1;
    } else if (entry->lastmod < request->ims) {
	debug(33, 3) ("-->  NO: entry older than client\n");
	return 0;
    } else if (request->imslen < 0) {
	debug(33, 3) ("-->  NO: same LMT, no client length\n");
	return 0;
    } else if (request->imslen == object_length) {
	debug(33, 3) ("-->  NO: same LMT, same length\n");
	return 0;
    } else {
	debug(33, 3) ("--> YES: same LMT, different length\n");
	return 1;
    }
}

char *
clientConstructTraceEcho(clientHttpRequest * http)
{
    LOCAL_ARRAY(char, line, 256);
    LOCAL_ARRAY(char, buf, 8192);
    size_t len;
    memset(buf, '\0', 8192);
    snprintf(buf, 8192, "HTTP/1.0 200 OK\r\n");
    snprintf(line, 256, "Date: %s\r\n", mkrfc1123(squid_curtime));
    strcat(buf, line);
    snprintf(line, 256, "Server: Squid/%s\r\n", SQUID_VERSION);
    strcat(buf, line);
    snprintf(line, 256, "Content-Type: message/http\r\n");
    strcat(buf, line);
    strcat(buf, "\r\n");
    len = strlen(buf);
    httpBuildRequestHeader(http->request,
	http->request,
	NULL,			/* entry */
	NULL,			/* in_len */
	buf + len,
	8192 - len,
	http->conn->fd,
	0);			/* flags */
    http->log_type = LOG_TCP_MISS;
    http->http_code = HTTP_OK;
    return buf;
}

void
clientPurgeRequest(clientHttpRequest * http)
{
    int fd = http->conn->fd;
    StoreEntry *entry;
    ErrorState *err = NULL;
    const cache_key *k;
    MemBuf mb;
    debug(33, 3) ("Config.onoff.enable_purge = %d\n", Config.onoff.enable_purge);
    if (!Config.onoff.enable_purge) {
	http->log_type = LOG_TCP_DENIED;
	err = errorCon(ERR_ACCESS_DENIED, HTTP_FORBIDDEN);
	err->request = requestLink(http->request);
	err->src_addr = http->conn->peer.sin_addr;
	http->entry = clientCreateStoreEntry(http, http->request->method, 0);
	errorAppendEntry(http->entry, err);
	return;
    }
    http->log_type = LOG_TCP_MISS;
    k = storeKeyPublic(http->uri, METHOD_GET);
    if ((entry = storeGet(k)) == NULL) {
	http->http_code = HTTP_NOT_FOUND;
    } else {
	storeRelease(entry);
	http->http_code = HTTP_OK;
    }
    mb = httpPackedReply(1.0, http->http_code, NULL, 0, 0, -1);
    comm_write_mbuf(fd, mb, clientWriteComplete, http);
}

int
checkNegativeHit(StoreEntry * e)
{
    if (!EBIT_TEST(e->flag, ENTRY_NEGCACHED))
	return 0;
    if (e->expires <= squid_curtime)
	return 0;
    if (e->store_status != STORE_OK)
	return 0;
    return 1;
}

void
clientUpdateCounters(clientHttpRequest * http)
{
    int svc_time = tvSubMsec(http->start, current_time);
    icp_ping_data *i;
    Counter.client_http.requests++;
    kb_incr(&Counter.client_http.kbytes_in, http->req_sz);
    kb_incr(&Counter.client_http.kbytes_out, http->out.size);
    if (isTcpHit(http->log_type)) {
	Counter.client_http.hits++;
	kb_incr(&Counter.client_http.hit_kbytes_out, http->out.size);
    }
    if (http->request->err_type != ERR_NONE)
	Counter.client_http.errors++;
    statHistCount(&Counter.client_http.all_svc_time, svc_time);
    /*
     * The idea here is not to be complete, but to get service times
     * for only well-defined types.  For example, we don't include
     * LOG_TCP_REFRESH_FAIL_HIT because its not really a cache hit
     * (we *tried* to validate it, but failed).
     */
    switch (http->log_type) {
    case LOG_TCP_IMS_HIT:
	statHistCount(&Counter.client_http.nm_svc_time, svc_time);
	break;
    case LOG_TCP_HIT:
    case LOG_TCP_MEM_HIT:
	statHistCount(&Counter.client_http.hit_svc_time, svc_time);
	break;
    case LOG_TCP_MISS:
    case LOG_TCP_CLIENT_REFRESH_MISS:
	statHistCount(&Counter.client_http.miss_svc_time, svc_time);
	break;
    default:
	/* make compiler warnings go away */
	break;
    }
    i = &http->request->hier.icp;
    if (0 != i->stop.tv_sec)
	statHistCount(&Counter.icp.query_svc_time, tvSubUsec(i->start, i->stop));
}

static void
httpRequestFree(void *data)
{
    clientHttpRequest *http = data;
    clientHttpRequest **H;
    ConnStateData *conn = http->conn;
    StoreEntry *entry = http->entry;
    request_t *request = http->request;
    MemObject *mem = NULL;
    debug(33, 3) ("httpRequestFree: %s\n", storeUrl(entry));
    if (!clientCheckTransferDone(http)) {
	if (entry)
	    storeUnregister(entry, http);	/* unregister BEFORE abort */
	CheckQuickAbort(http);
	entry = http->entry;	/* reset, IMS might have changed it */
	if (entry && entry->ping_status == PING_WAITING)
	    storeReleaseRequest(entry);
	protoUnregister(entry, request);
    }
    assert(http->log_type < LOG_TYPE_MAX);
    if (entry)
	mem = entry->mem_obj;
    if (http->out.size || http->log_type) {
	http->al.icp.opcode = 0;
	http->al.url = http->log_uri;
	debug(33, 9) ("httpRequestFree: al.url='%s'\n", http->al.url);
	if (mem) {
	    http->al.http.code = mem->reply->sline.status;
	    http->al.http.content_type = strBuf(mem->reply->content_type);
	}
	http->al.cache.caddr = conn->log_addr;
	http->al.cache.size = http->out.size;
	http->al.cache.code = http->log_type;
	http->al.cache.msec = tvSubMsec(http->start, current_time);
	http->al.cache.ident = conn->ident.ident;
	if (request) {
	    http->al.http.method = request->method;
	    http->al.headers.request = request->headers;
	    http->al.hier = request->hier;
	}
	accessLogLog(&http->al);
	clientUpdateCounters(http);
	clientdbUpdate(conn->peer.sin_addr, http->log_type, PROTO_HTTP, http->out.size);
    }
    if (http->redirect_state == REDIRECT_PENDING)
	redirectUnregister(http->uri, http);
    if (http->acl_checklist)
	aclChecklistFree(http->acl_checklist);
    if (request)
	checkFailureRatio(request->err_type, http->al.hier.code);
    safe_free(http->uri);
    safe_free(http->log_uri);
    safe_free(http->al.headers.reply);
    if (entry) {
	http->entry = NULL;
	storeUnregister(entry, http);
	storeUnlockObject(entry);
    }
    /* old_entry might still be set if we didn't yet get the reply
     * code in clientHandleIMSReply() */
    if (http->old_entry) {
	storeUnregister(http->old_entry, http);
	storeUnlockObject(http->old_entry);
	http->old_entry = NULL;
    }
    requestUnlink(http->request);
    assert(http != http->next);
    assert(http->conn->chr != NULL);
    H = &http->conn->chr;
    while (*H) {
	if (*H == http)
	    break;
	H = &(*H)->next;
    }
    assert(*H != NULL);
    *H = http->next;
    http->next = NULL;
    cbdataFree(http);
}

/* This is a handler normally called by comm_close() */
static void
connStateFree(int fd, void *data)
{
    ConnStateData *connState = data;
    clientHttpRequest *http;
    debug(33, 3) ("connStateFree: FD %d\n", fd);
    assert(connState != NULL);
    while ((http = connState->chr) != NULL) {
	assert(http->conn == connState);
	assert(connState->chr != connState->chr->next);
	httpRequestFree(http);
    }
    if (connState->ident.fd > -1)
	comm_close(connState->ident.fd);
    safe_free(connState->in.buf);
    /* XXX account connState->in.buf */
    pconnHistCount(0, connState->nrequests);
    cbdataFree(connState);
}

static void
clientParseRequestHeaders(clientHttpRequest * http)
{
    request_t *request = http->request;
    char *request_hdr = request->headers;
    char *t = NULL;
    request->ims = -2;
    request->imslen = -1;
    if ((t = mime_get_header(request_hdr, "If-Modified-Since"))) {
	EBIT_SET(request->flags, REQ_IMS);
	request->ims = parse_rfc1123(t);
	while ((t = strchr(t, ';'))) {
	    for (t++; isspace(*t); t++);
	    if (strncasecmp(t, "length=", 7) == 0)
		request->imslen = atoi(t + 7);
	}
    }
    if ((t = mime_get_header(request_hdr, "Pragma"))) {
	if (!strcasecmp(t, "no-cache"))
	    EBIT_SET(request->flags, REQ_NOCACHE);
    }
    if (mime_get_header(request_hdr, "Range")) {
	EBIT_SET(request->flags, REQ_NOCACHE);
	EBIT_SET(request->flags, REQ_RANGE);
    } else if (mime_get_header(request_hdr, "Request-Range")) {
	EBIT_SET(request->flags, REQ_NOCACHE);
	EBIT_SET(request->flags, REQ_RANGE);
    }
    if (mime_get_header(request_hdr, "Authorization"))
	EBIT_SET(request->flags, REQ_AUTH);
    if (request->login[0] != '\0')
	EBIT_SET(request->flags, REQ_AUTH);
    if ((t = mime_get_header(request_hdr, "Proxy-Connection"))) {
	if (!strcasecmp(t, "Keep-Alive"))
	    EBIT_SET(request->flags, REQ_PROXY_KEEPALIVE);
    }
    if ((t = mime_get_header(request_hdr, "Via"))) {
	if (strstr(t, ThisCache)) {
	    if (!http->accel) {
		debug(33, 1) ("WARNING: Forwarding loop detected for '%s'\n",
		    http->uri);
		debug(33, 1) ("--> %s\n", t);
	    }
	    EBIT_SET(request->flags, REQ_LOOPDETECT);
	}
#if FORW_VIA_DB
	fvdbCountVia(t);
#endif
    }
#if USE_USERAGENT_LOG
    if ((t = mime_get_header(request_hdr, "User-Agent")))
	logUserAgent(fqdnFromAddr(http->conn->peer.sin_addr), t);
#endif
#if FORW_VIA_DB
    if ((t = mime_get_header(request_hdr, "X-Forwarded-For")))
	fvdbCountForw(t);
#endif
    request->max_age = -1;
    if ((t = mime_get_header(request_hdr, "Cache-control"))) {
	if (!strncasecmp(t, "Max-age=", 8))
	    request->max_age = atoi(t + 8);
    }
    if (request->method == METHOD_TRACE) {
	if ((t = mime_get_header(request_hdr, "Max-Forwards")))
	    request->max_forwards = atoi(t);
    }
    if (clientCachable(http))
	EBIT_SET(request->flags, REQ_CACHABLE);
    if (clientHierarchical(http))
	EBIT_SET(request->flags, REQ_HIERARCHICAL);
    debug(33, 5) ("clientParseRequestHeaders: REQ_NOCACHE = %s\n",
	EBIT_TEST(request->flags, REQ_NOCACHE) ? "SET" : "NOT SET");
    debug(33, 5) ("clientParseRequestHeaders: REQ_CACHABLE = %s\n",
	EBIT_TEST(request->flags, REQ_CACHABLE) ? "SET" : "NOT SET");
    debug(33, 5) ("clientParseRequestHeaders: REQ_HIERARCHICAL = %s\n",
	EBIT_TEST(request->flags, REQ_HIERARCHICAL) ? "SET" : "NOT SET");
}

static int
clientCheckContentLength(request_t * r)
{
    char *t;
    int len;
    /*
     * We only require a content-length for "upload" methods
     */
    if (0 == pumpMethod(r->method))
	return 1;
    t = mime_get_header(r->headers, "Content-Length");
    if (NULL == t)
	return 0;
    len = atoi(t);
    if (len < 0)
	return 0;
    return 1;
}

static int
clientCachable(clientHttpRequest * http)
{
    const char *url = http->uri;
    request_t *req = http->request;
    method_t method = req->method;
    const wordlist *p;
    for (p = Config.cache_stoplist; p; p = p->next) {
	if (strstr(url, p->key))
	    return 0;
    }
    if (Config.cache_stop_relist)
	if (aclMatchRegex(Config.cache_stop_relist, url))
	    return 0;
    if (req->protocol == PROTO_HTTP)
	return httpCachable(method);
    /* FTP is always cachable */
    if (req->protocol == PROTO_GOPHER)
	return gopherCachable(url);
    if (req->protocol == PROTO_WAIS)
	return 0;
    if (method == METHOD_CONNECT)
	return 0;
    if (method == METHOD_TRACE)
	return 0;
    if (req->protocol == PROTO_CACHEOBJ)
	return 0;
    return 1;
}

/* Return true if we can query our neighbors for this object */
static int
clientHierarchical(clientHttpRequest * http)
{
    const char *url = http->uri;
    request_t *request = http->request;
    method_t method = request->method;
    const wordlist *p = NULL;

    /* IMS needs a private key, so we can use the hierarchy for IMS only
     * if our neighbors support private keys */
    if (EBIT_TEST(request->flags, REQ_IMS) && !neighbors_do_private_keys)
	return 0;
    if (EBIT_TEST(request->flags, REQ_AUTH))
	return 0;
    if (method == METHOD_TRACE)
	return 1;
    if (method != METHOD_GET)
	return 0;
    /* scan hierarchy_stoplist */
    for (p = Config.hierarchy_stoplist; p; p = p->next)
	if (strstr(url, p->key))
	    return 0;
    if (EBIT_TEST(request->flags, REQ_LOOPDETECT))
	return 0;
    if (request->protocol == PROTO_HTTP)
	return httpCachable(method);
    if (request->protocol == PROTO_GOPHER)
	return gopherCachable(url);
    if (request->protocol == PROTO_WAIS)
	return 0;
    if (request->protocol == PROTO_CACHEOBJ)
	return 0;
    return 1;
}

int
isTcpHit(log_type code)
{
    /* this should be a bitmap for better optimization */
    if (code == LOG_TCP_HIT)
	return 1;
    if (code == LOG_TCP_IMS_HIT)
	return 1;
    if (code == LOG_TCP_REFRESH_FAIL_HIT)
	return 1;
    if (code == LOG_TCP_REFRESH_HIT)
	return 1;
    if (code == LOG_TCP_NEGATIVE_HIT)
	return 1;
    if (code == LOG_TCP_MEM_HIT)
	return 1;
    return 0;
}

static void
clientAppendReplyHeader(char *hdr, const char *line, size_t * sz, size_t max)
{
    size_t n = *sz + strlen(line) + 2;
    if (n >= max)
	return;
    strcpy(hdr + (*sz), line);
    strcat(hdr + (*sz), crlf);
    *sz = n;
}

/* this entire function has to be rewriten using new interfaces @?@ @?@ */
size_t
clientBuildReplyHeader(clientHttpRequest * http,
    char *hdr_in,
    size_t hdr_in_sz,
    size_t * in_len,
    char *hdr_out,
    size_t out_sz)
{
    LOCAL_ARRAY(char, no_forward, 1024);
    char *xbuf;
    char *ybuf;
    char *t = NULL;
    char *end;
    size_t len = 0;
    size_t hdr_len = 0;
    size_t l;
    if (0 != strncmp(hdr_in, "HTTP/", 5))
	return 0;
    hdr_len = headersEnd(hdr_in, hdr_in_sz);
    if (0 == hdr_len) {
	debug(33, 3) ("clientBuildReplyHeader: DIDN'T FIND END-OF-HEADERS\n");
	return 0;
    }
    xbuf = memAllocate(MEM_4K_BUF);
    ybuf = memAllocate(MEM_4K_BUF);
    end = hdr_in + hdr_len;
    for (t = hdr_in; t < end; t += strcspn(t, crlf), t += strspn(t, crlf)) {
	l = strcspn(t, crlf) + 1;
	xstrncpy(xbuf, t, l > 4096 ? 4096 : l);
	/* enforce 1.0 reply version, this hack will be rewritten */
	if (t == hdr_in && !strncasecmp(xbuf, "HTTP/", 5) && l > 8 &&
	    (isspace(xbuf[8]) || isspace(xbuf[9])))
	    xmemmove(xbuf + 5, "1.0 ", 4);
#if DONT_FILTER_THESE
	/*
	 * but you might want to if you run Squid as a HTTP accelerator
	 */
	if (strncasecmp(xbuf, "Accept-Ranges:", 14) == 0)
	    continue;
	if (strncasecmp(xbuf, "Etag:", 5) == 0)
	    continue;
#endif
	if (strncasecmp(xbuf, "Proxy-Connection:", 17) == 0)
	    continue;
	if (strncasecmp(xbuf, "Connection:", 11) == 0) {
	    handleConnectionHeader(0, no_forward, &xbuf[11]);
	    continue;
	}
	if (strncasecmp(xbuf, "Keep-Alive:", 11) == 0)
	    continue;
	if (strncasecmp(xbuf, "Set-Cookie:", 11) == 0)
	    if (isTcpHit(http->log_type))
		continue;
	if (!handleConnectionHeader(1, no_forward, xbuf))
	    clientAppendReplyHeader(hdr_out, xbuf, &len, out_sz - 512);
    }
    /* Append X-Cache: */
    snprintf(ybuf, 4096, "X-Cache: %s from %s",
	isTcpHit(http->log_type) ? "HIT" : "MISS",
	getMyHostname());
    clientAppendReplyHeader(hdr_out, ybuf, &len, out_sz);
    /* Append Proxy-Connection: */
    if (EBIT_TEST(http->request->flags, REQ_PROXY_KEEPALIVE)) {
	snprintf(ybuf, 4096, "Proxy-Connection: Keep-Alive");
	clientAppendReplyHeader(hdr_out, ybuf, &len, out_sz);
    }
    clientAppendReplyHeader(hdr_out, null_string, &len, out_sz);
    if (in_len)
	*in_len = hdr_len;
    if ((l = strlen(hdr_out)) != len) {
	debug_trap("clientBuildReplyHeader: size mismatch");
	len = l;
    }
    debug(33, 3) ("clientBuildReplyHeader: OUTPUT:\n%s\n", hdr_out);
    memFree(MEM_4K_BUF, xbuf);
    memFree(MEM_4K_BUF, ybuf);
    /* temporary kludge to test headers, remove it @?@ @?@ */
    {
	extern void httpHeaderTestParser(const char *hstr);
	httpHeaderTestParser(hdr_out);
    }
    /* end of kludge */
    return len;
}

static void
clientCacheHit(void *data, char *buf, ssize_t size)
{
    clientHttpRequest *http = data;
    debug(33, 3) ("clientCacheHit: %s, %d bytes\n", http->uri, (int) size);
    if (size >= 0) {
	clientSendMoreData(data, buf, size);
    } else if (http->entry == NULL) {
	debug(33, 3) ("clientCacheHit: request aborted\n");
    } else {
	/* swap in failure */
	debug(33, 3) ("clientCacheHit: swapin failure for %s\n", http->uri);
	http->log_type = LOG_TCP_SWAPFAIL_MISS;
	if (http->entry)
	    storeRelease(http->entry);
	clientProcessMiss(http);
    }
}

static void
clientSendMoreData(void *data, char *buf, ssize_t size)
{
    clientHttpRequest *http = data;
    StoreEntry *entry = http->entry;
    ConnStateData *conn = http->conn;
    int fd = conn->fd;
    size_t hdrlen;
    size_t l = 0;
    size_t writelen;
    char *newbuf;
    FREE *freefunc = memFree4K;
    debug(33, 5) ("clientSendMoreData: %s, %d bytes\n", http->uri, (int) size);
    assert(size <= SM_PAGE_SIZE);
    assert(http->request != NULL);
    debug(33, 5) ("clientSendMoreData: FD %d '%s', out.offset=%d \n",
	fd, storeUrl(entry), (int) http->out.offset);
    if (conn->chr != http) {
	/* there is another object in progress, defer this one */
	debug(0, 0) ("clientSendMoreData: Deferring %s\n", storeUrl(entry));
	freefunc(buf);
	return;
    } else if (entry->store_status == STORE_ABORTED) {
	/* call clientWriteComplete so the client socket gets closed */
	clientWriteComplete(fd, NULL, 0, COMM_OK, http);
	freefunc(buf);
	return;
    } else if (size < 0) {
	/* call clientWriteComplete so the client socket gets closed */
	clientWriteComplete(fd, NULL, 0, COMM_OK, http);
	freefunc(buf);
	return;
    } else if (size == 0) {
	/* call clientWriteComplete so the client socket gets closed */
	clientWriteComplete(fd, NULL, 0, COMM_OK, http);
	freefunc(buf);
	return;
    }
    writelen = size;
    if (http->out.offset == 0) {
	if (Config.onoff.log_mime_hdrs) {
	    size_t k;
	    if ((k = headersEnd(buf, size))) {
		safe_free(http->al.headers.reply);
		http->al.headers.reply = xcalloc(k + 1, 1);
		xstrncpy(http->al.headers.reply, buf, k);
	    }
	}
	newbuf = memAllocate(MEM_8K_BUF);
	hdrlen = 0;
	l = clientBuildReplyHeader(http, buf, size, &hdrlen, newbuf, 8192);
	if (l != 0) {
	    writelen = l + size - hdrlen;
	    assert(writelen <= 8192);
	    /*
	     * l is the length of the new headers in newbuf
	     * hdrlen is the length of the old headers in buf
	     * size - hdrlen is the amount of body in buf
	     */
	    debug(33, 3) ("clientSendMoreData: Appending %d bytes after headers\n",
		(int) (size - hdrlen));
	    if (((size - hdrlen) + l) > 8192) {
		debug(0, 0) ("Size, hdrlen, l %d, %d, %d\n", size, hdrlen, l);
		return;
	    }
	    xmemcpy(newbuf + l, buf + hdrlen, size - hdrlen);
	    /* replace buf with newbuf */
	    freefunc(buf);
	    buf = newbuf;
	    freefunc = memFree8K;
	    newbuf = NULL;
	} else {
	    memFree(MEM_8K_BUF, newbuf);
	    newbuf = NULL;
	    if (size < SM_PAGE_SIZE && entry->store_status == STORE_PENDING) {
		/* wait for more to arrive */
		storeClientCopy(entry,
		    http->out.offset + size,
		    http->out.offset,
		    SM_PAGE_SIZE,
		    buf,
		    clientSendMoreData,
		    http);
		return;
	    }
	}
    }
    http->out.offset += size;
    /*
     * ick, this is gross
     */
    if (http->request->method == METHOD_HEAD) {
	size_t k;
	if ((k = headersEnd(buf, size))) {
	    writelen = k;
	    /* force end */
	    if (entry->store_status == STORE_PENDING)
		http->out.offset = entry->mem_obj->inmem_hi;
	    else
		http->out.offset = objectLen(entry);
	}
    }
    comm_write(fd, buf, writelen, clientWriteComplete, http, freefunc);
}

static void
clientWriteComplete(int fd, char *bufnotused, size_t size, int errflag, void *data)
{
    clientHttpRequest *http = data;
    ConnStateData *conn;
    StoreEntry *entry = http->entry;
    int done;
    http->out.size += size;
    debug(33, 5) ("clientWriteComplete: FD %d, sz %d, err %d, off %d, len %d\n",
	fd, size, errflag, (int) http->out.offset, objectLen(entry));
    if (errflag) {
#if DONT_DO_THIS
	/*
	 * Not sure why this CheckQuickAbort() would be needed here.
	 * We also call CheckQuickAbort() in httpRequestFree(), which
	 * gets called as a comm_close handler.  We need to be careful
	 * that CheckQuickAbort() gets called only ONCE, and AFTER
	 * storeUnregister() has been called.  [DW/1.2.beta18]
	 */
	CheckQuickAbort(http);
#endif
	comm_close(fd);
    } else if (entry->store_status == STORE_ABORTED) {
	comm_close(fd);
    } else if ((done = clientCheckTransferDone(http)) != 0 || size == 0) {
	debug(33, 5) ("clientWriteComplete: FD %d transfer is DONE\n", fd);
	/* We're finished case */
	if (http->entry->mem_obj->reply->content_length < 0 || !done ||
	    EBIT_TEST(entry->flag, ENTRY_BAD_LENGTH)) {
	    /* 
	     * Client connection closed due to unknown or invalid
	     * content length. Persistent connection is not possible.
	     * This catches most cases, but probably not all.
	     */
	    comm_close(fd);
	} else if (EBIT_TEST(http->request->flags, REQ_PROXY_KEEPALIVE)) {
	    debug(33, 5) ("clientWriteComplete: FD %d Keeping Alive\n", fd);
	    conn = http->conn;
	    conn->defer.until = 0;	/* Kick it to read a new request */
	    httpRequestFree(http);
	    if ((http = conn->chr) != NULL) {
		debug(33, 1) ("clientWriteComplete: FD %d Sending next request\n", fd);
		if (!storeClientCopyPending(http->entry, http)) {
		    if (entry->store_status == STORE_ABORTED)
			debug(33, 0) ("clientWriteComplete: entry->swap_status == STORE_ABORTED\n");
		    storeClientCopy(http->entry,
			http->out.offset,
			http->out.offset,
			SM_PAGE_SIZE,
			memAllocate(MEM_4K_BUF),
			clientSendMoreData,
			http);
		}
	    } else {
		debug(33, 5) ("clientWriteComplete: FD %d reading next request\n", fd);
		fd_note(fd, "Reading next request");
		/*
		 * Set the timeout BEFORE calling clientReadRequest().
		 */
		commSetTimeout(fd, 15, requestTimeout, conn);
		clientReadRequest(fd, conn);	/* Read next request */
		/*
		 * Note, the FD may be closed at this point.
		 */
	    }
	} else {
	    comm_close(fd);
	}
    } else {
	/* More data will be coming from primary server; register with 
	 * storage manager. */
	if (entry->store_status == STORE_ABORTED)
	    debug(33, 0) ("clientWriteComplete 2: entry->swap_status == STORE_ABORTED\n");
	storeClientCopy(entry,
	    http->out.offset,
	    http->out.offset,
	    SM_PAGE_SIZE,
	    memAllocate(MEM_4K_BUF),
	    clientSendMoreData,
	    http);
    }
}

static void
clientGetHeadersForIMS(void *data, char *buf, ssize_t size)
{
    clientHttpRequest *http = data;
    StoreEntry *entry = http->entry;
    MemObject *mem;
    debug(33, 3) ("clientGetHeadersForIMS: %s, %d bytes\n",
	http->uri, (int) size);
    assert(size <= SM_PAGE_SIZE);
    memFree(MEM_4K_BUF, buf);
    buf = NULL;
    if (size < 0 || entry->store_status == STORE_ABORTED) {
	/*
	 * There are different reasons why we might have size < 0.  One
	 * being that we failed to open a swapfile.  Another being that
	 * the request was cancelled from the client-side.  If the client
	 * cancelled the request, then http->entry will be NULL.
	 */
	if (entry != NULL) {
	    debug(33, 1) ("clientGetHeadersForIMS: storeClientCopy failed for '%s'\n",
		storeKeyText(entry->key));
	    clientProcessMiss(http);
	}
	return;
    }
    mem = entry->mem_obj;
    if (mem->reply->sline.status == 0) {
	if (entry->mem_status == IN_MEMORY) {
	    clientProcessMiss(http);
	    return;
	}
	if (size == SM_PAGE_SIZE && http->out.offset == 0) {
	    /*
	     * We can't get any more headers than this, so bail
	     */
	    debug(33, 1) ("clientGetHeadersForIMS: failed, forcing cache miss\n");
	    clientProcessMiss(http);
	    return;
	}
	debug(33, 3) ("clientGetHeadersForIMS: waiting for HTTP reply headers\n");
	/* All headers are not yet available, wait for more data */
	if (entry->store_status == STORE_ABORTED)
	    debug(33, 0) ("clientGetHeadersForIMS: entry->swap_status == STORE_ABORTED\n");
	storeClientCopy(entry,
	    http->out.offset + size,
	    http->out.offset,
	    SM_PAGE_SIZE,
	    memAllocate(MEM_4K_BUF),
	    clientGetHeadersForIMS,
	    http);
	return;
    }
    /* All headers are available, check if object is modified or not */
    /* ---------------------------------------------------------------
     * Removed check for reply->sline.status != 200 because of a potential
     * problem with ICP.  We will return a HIT for any public, cached
     * object.  This includes other responses like 301, 410, as coded in
     * http.c.  It is Bad(tm) to return UDP_HIT and then, if the reply
     * code is not 200, hand off to clientProcessMiss(), which may disallow
     * the request based on 'miss_access' rules.  Alternatively, we might
     * consider requiring returning UDP_HIT only for 200's.  This
     * problably means an entry->flag bit, which would be lost during
     * restart because the flags aren't preserved across restarts.
     * --DW 3/11/96.
     * ---------------------------------------------------------------- */
#ifdef CHECK_REPLY_CODE_NOTEQUAL_200
    /* Only objects with statuscode==200 can be "Not modified" */
    if (mem->reply->sline.status != 200) {
	debug(33, 4) ("clientGetHeadersForIMS: Reply code %d!=200\n",
	    mem->reply->sline.status);
	clientProcessMiss(http);
	return;
    }
#endif
    http->log_type = LOG_TCP_IMS_HIT;
    entry->refcount++;
    if (modifiedSince(entry, http->request)) {
	if (entry->store_status == STORE_ABORTED)
	    debug(33, 0) ("clientGetHeadersForIMS 2: entry->swap_status == STORE_ABORTED\n");
	storeClientCopy(entry,
	    http->out.offset,
	    http->out.offset,
	    SM_PAGE_SIZE,
	    memAllocate(MEM_4K_BUF),
	    clientSendMoreData,
	    http);
	return;
    }
    debug(33, 4) ("clientGetHeadersForIMS: Not modified '%s'\n", storeUrl(entry));
    comm_write_mbuf(http->conn->fd, httpPacked304Reply(mem->reply),
	clientHandleIMSComplete, http);
}

static void
clientHandleIMSComplete(int fd, char *bufnotused, size_t size, int flag, void *data)
{
    clientHttpRequest *http = data;
    StoreEntry *entry = http->entry;
    debug(33, 5) ("clientHandleIMSComplete: Not Modified sent '%s'\n", storeUrl(entry));
    /* Set up everything for the logging */
    storeUnregister(entry, http);
    storeUnlockObject(entry);
    http->entry = NULL;
    http->out.size += size;
    http->al.http.code = 304;
    if (flag != COMM_ERR_CLOSING)
	comm_close(fd);
}

/*
 * client issued a request with an only-if-cached cache-control directive
 * we did not find a cached object that can be returned without
 *     contacting other servers
 * respond with a 504 (Gateway Timeout) as suggested in [RFC 2068]
 */
static void
clientProcessOnlyIfCachedMiss(clientHttpRequest * http)
{
    char *url = http->uri;
    request_t *r = http->request;
    ErrorState *err = NULL;
    debug(33, 4) ("clientProcessOnlyIfCachedMiss: '%s %s'\n",
	RequestMethodStr[r->method], url);
    debug(33, 10) ("clientProcessMiss: request_hdr:\n%s\n", r->headers);
    http->al.http.code = HTTP_GATEWAY_TIMEOUT;
    err = errorCon(ERR_ONLY_IF_CACHED_MISS, HTTP_GATEWAY_TIMEOUT);
    err->request = requestLink(r);
    err->src_addr = http->conn->peer.sin_addr;
    http->entry = clientCreateStoreEntry(http, r->method, 0);
    errorAppendEntry(http->entry, err);
}

static log_type
clientProcessRequest2(clientHttpRequest * http)
{
    const request_t *r = http->request;
    const cache_key *key = storeKeyPublic(http->uri, r->method);
    StoreEntry *e;
    if ((e = http->entry = storeGet(key)) == NULL) {
	/* this object isn't in the cache */
	return LOG_TCP_MISS;
    } else if (EBIT_TEST(e->flag, ENTRY_SPECIAL)) {
	if (e->mem_status == IN_MEMORY)
	    return LOG_TCP_MEM_HIT;
	else
	    return LOG_TCP_HIT;
    } else if (!storeEntryValidToSend(e)) {
	storeRelease(e);
	http->entry = NULL;
	return LOG_TCP_MISS;
    } else if (EBIT_TEST(r->flags, REQ_NOCACHE)) {
	/* NOCACHE should always eject a negative cached object */
	if (EBIT_TEST(e->flag, ENTRY_NEGCACHED))
	    storeRelease(e);
	/* NOCACHE+IMS should not eject a valid object */
	else if (EBIT_TEST(r->flags, REQ_IMS))
	    (void) 0;
	/* Request-Range should not eject a valid object */
	else if (EBIT_TEST(r->flags, REQ_RANGE))
	    (void) 0;
	else
	    storeRelease(e);
	ipcacheReleaseInvalid(r->host);
	http->entry = NULL;
	return LOG_TCP_CLIENT_REFRESH_MISS;
    } else if (checkNegativeHit(e)) {
	return LOG_TCP_NEGATIVE_HIT;
    } else if (refreshCheck(e, r, 0)) {
	/* The object is in the cache, but it needs to be validated.  Use
	 * LOG_TCP_REFRESH_MISS for the time being, maybe change it to
	 * _HIT later in clientHandleIMSReply() */
	if (r->protocol == PROTO_HTTP)
	    return LOG_TCP_REFRESH_MISS;
	else
	    return LOG_TCP_MISS;	/* XXX zoinks */
    } else if (EBIT_TEST(r->flags, REQ_IMS)) {
	/* User-initiated IMS request for something we think is valid */
	return LOG_TCP_IMS_MISS;
    } else if (e->mem_status == IN_MEMORY) {
	return LOG_TCP_MEM_HIT;
    } else {
	return LOG_TCP_HIT;
    }
}

static void
clientProcessRequest(clientHttpRequest * http)
{
    char *url = http->uri;
    StoreEntry *entry = NULL;
    request_t *r = http->request;
    int fd = http->conn->fd;
    HttpReply *rep;
    debug(33, 4) ("clientProcessRequest: %s '%s'\n",
	RequestMethodStr[r->method],
	url);
    if (r->method == METHOD_CONNECT) {
	http->log_type = LOG_TCP_MISS;
	sslStart(fd, url, r, &http->out.size);
	return;
    } else if (r->method == METHOD_PURGE) {
	clientPurgeRequest(http);
	return;
    } else if (r->method == METHOD_TRACE) {
	if (r->max_forwards == 0) {
	    http->entry = clientCreateStoreEntry(http, r->method, 0);
	    storeReleaseRequest(http->entry);
	    storeBuffer(http->entry);
	    rep = httpReplyCreate();
	    httpReplySetHeaders(rep, 1.0, HTTP_OK, NULL, "text/plain",
		r->headers_sz, 0, squid_curtime);
	    httpReplySwapOut(rep, http->entry);
	    httpReplyDestroy(rep);
	    storeAppend(http->entry, r->headers, r->headers_sz);
	    storeComplete(http->entry);
	    return;
	}
	/* yes, continue */
	http->log_type = LOG_TCP_MISS;
    } else if (pumpMethod(r->method)) {
	http->log_type = LOG_TCP_MISS;
	/* XXX oof, POST can be cached! */
	pumpInit(fd, r, http->uri);
    } else {
	http->log_type = clientProcessRequest2(http);
    }
    debug(33, 4) ("clientProcessRequest: %s for '%s'\n",
	log_tags[http->log_type],
	http->uri);
    if ((entry = http->entry) != NULL) {
	storeLockObject(entry);
	storeCreateMemObject(entry, http->uri, http->log_uri);
	storeClientListAdd(entry, http);
    }
    http->out.offset = 0;
    /* "pure" hit? (will not contact other servers) */
    switch (http->log_type) {
    case LOG_TCP_HIT:
    case LOG_TCP_NEGATIVE_HIT:
    case LOG_TCP_MEM_HIT:
	entry->refcount++;	/* HIT CASE */
	if (entry->store_status == STORE_ABORTED)
	    debug(33, 0) ("clientProcessRequest: entry->swap_status == STORE_ABORTED\n");
	storeClientCopy(entry,
	    http->out.offset,
	    http->out.offset,
	    SM_PAGE_SIZE,
	    memAllocate(MEM_4K_BUF),
	    clientCacheHit,
	    http);
	return;
    default:
	break;
    }
    /* ok, it is a miss or a "dirty" hit (will contact other servers) */
    /* are we allowed to contact other servers? */
    if (r->cache_control && EBIT_TEST(r->cache_control->mask, CC_ONLY_IF_CACHED)) {
	/* nope, bailing out */
	clientProcessOnlyIfCachedMiss(http);
	return;
    }
    switch (http->log_type) {
    case LOG_TCP_IMS_MISS:
	if (entry->store_status == STORE_ABORTED)
	    debug(33, 0) ("clientProcessRequest 2: entry->swap_status == STORE_ABORTED\n");
	storeClientCopy(entry,
	    http->out.offset,
	    http->out.offset,
	    SM_PAGE_SIZE,
	    memAllocate(MEM_4K_BUF),
	    clientGetHeadersForIMS,
	    http);
	break;
    case LOG_TCP_REFRESH_MISS:
	clientProcessExpired(http);
	break;
    default:
	clientProcessMiss(http);
	break;
    }
}

/*
 * Prepare to fetch the object as it's a cache miss of some kind.
 */
static void
clientProcessMiss(clientHttpRequest * http)
{
    char *url = http->uri;
    request_t *r = http->request;
    char *request_hdr = r->headers;
    aclCheck_t ch;
    int answer;
    ErrorState *err = NULL;
    debug(33, 4) ("clientProcessMiss: '%s %s'\n",
	RequestMethodStr[r->method], url);
    debug(33, 10) ("clientProcessMiss: request_hdr:\n%s\n", request_hdr);
    /*
     * We might have a left-over StoreEntry from a failed cache hit
     * or IMS request.
     */
    if (http->entry) {
	storeUnregister(http->entry, http);
	storeUnlockObject(http->entry);
	http->entry = NULL;
    }
    /*
     * Check if this host is allowed to fetch MISSES from us (miss_access)
     */
    memset(&ch, '\0', sizeof(aclCheck_t));
    ch.src_addr = http->conn->peer.sin_addr;
    ch.request = r;
    answer = aclCheckFast(Config.accessList.miss, &ch);
    if (answer == 0 || http->internal) {
	http->al.http.code = HTTP_FORBIDDEN;
	err = errorCon(ERR_FORWARDING_DENIED, HTTP_FORBIDDEN);
	err->request = requestLink(r);
	err->src_addr = http->conn->peer.sin_addr;
	http->entry = clientCreateStoreEntry(http, r->method, 0);
	errorAppendEntry(http->entry, err);
	return;
    }
    assert(http->out.offset == 0);
    http->entry = clientCreateStoreEntry(http, r->method, r->flags);
    http->entry->mem_obj->fd = http->conn->fd;
    http->entry->refcount++;
    protoDispatch(http->conn->fd, http->entry, r);
}

/*
 *  parseHttpRequest()
 * 
 *  Returns
 *   NULL on error or incomplete request
 *    a clientHttpRequest structure on success
 */
static clientHttpRequest *
parseHttpRequest(ConnStateData * conn, method_t * method_p, int *status,
    char **headers_p, size_t * headers_sz_p)
{
    char *inbuf = NULL;
    char *mstr = NULL;
    char *url = NULL;
    char *req_hdr = NULL;
    float http_ver;
    char *token = NULL;
    char *t = NULL;
    char *end;
    int free_request = 0;
    size_t header_sz;		/* size of headers, not including first line */
    size_t req_sz;		/* size of whole request */
    size_t url_sz;
    method_t method;
    clientHttpRequest *http = NULL;

    /* Make sure a complete line has been received */
    if (strchr(conn->in.buf, '\n') == NULL) {
	debug(33, 5) ("Incomplete request line, waiting for more data\n");
	*status = 0;
	return NULL;
    }
    /* Use xmalloc/xmemcpy instead of xstrdup because inbuf might
     * contain NULL bytes; especially for POST data  */
    inbuf = xmalloc(conn->in.offset + 1);
    xmemcpy(inbuf, conn->in.buf, conn->in.offset);
    *(inbuf + conn->in.offset) = '\0';

    /* Look for request method */
    if ((mstr = strtok(inbuf, "\t ")) == NULL) {
	debug(33, 1) ("parseHttpRequest: Can't get request method\n");
	http = xcalloc(1, sizeof(clientHttpRequest));
	cbdataAdd(http, MEM_NONE);
	http->conn = conn;
	http->start = current_time;
	http->req_sz = conn->in.offset;
	http->uri = xstrdup("error:invalid-request-method");
	http->log_uri = xstrdup("error:invalid-request-method");
	*headers_sz_p = conn->in.offset;
	*headers_p = inbuf;
	*method_p = METHOD_NONE;
	*status = -1;
	return http;
    }
    method = urlParseMethod(mstr);
    if (method == METHOD_NONE) {
	debug(33, 1) ("parseHttpRequest: Unsupported method '%s'\n", mstr);
	http = xcalloc(1, sizeof(clientHttpRequest));
	cbdataAdd(http, MEM_NONE);
	http->conn = conn;
	http->start = current_time;
	http->req_sz = conn->in.offset;
	http->uri = xstrdup("error:unsupported-request-method");
	http->log_uri = xstrdup("error:unsupported-request-method");
	*headers_sz_p = conn->in.offset;
	*headers_p = inbuf;
	*method_p = METHOD_NONE;
	*status = -1;
	return http;
    }
    debug(33, 5) ("parseHttpRequest: Method is '%s'\n", mstr);
    *method_p = method;

    /* look for URL */
    if ((url = strtok(NULL, "\r\n\t ")) == NULL) {
	debug(33, 1) ("parseHttpRequest: Missing URL\n");
	http = xcalloc(1, sizeof(clientHttpRequest));
	cbdataAdd(http, MEM_NONE);
	http->conn = conn;
	http->start = current_time;
	http->req_sz = conn->in.offset;
	http->uri = xstrdup("error:missing-url");
	http->log_uri = xstrdup("error:missing-url");
	*headers_sz_p = conn->in.offset;
	*headers_p = inbuf;
	*status = -1;
	return http;
    }
    debug(33, 5) ("parseHttpRequest: Request is '%s'\n", url);

    token = strtok(NULL, null_string);
    for (t = token; t && *t && *t != '\n' && *t != '\r'; t++);
    if (t == NULL || *t == '\0' || t == token || strncmp(token, "HTTP/", 5)) {
	debug(33, 3) ("parseHttpRequest: Missing HTTP identifier\n");
#if STRICT_HTTP_PARSER
	http = xcalloc(1, sizeof(clientHttpRequest));
	cbdataAdd(http, MEM_NONE);
	http->conn = conn;
	http->start = current_time;
	http->req_sz = conn->in.offset;
	http->uri = xstrdup("error:missing-http-ident");
	http->log_uri = xstrdup("error:missing-http-ident");
	*headers_sz_p = conn->in.offset;
	*headers_p = inbuf;
	*status = -1;
	return http;
#else
	http_ver = (float) 0.9;	/* wild guess */
#endif
    } else
	http_ver = (float) atof(token + 5);

    /*
     * Skip whitespace at the end of the frist line, up to the
     * first newline.
     */
    while (isspace(*t))
	if (*(t++) == '\n')
	    break;
    req_hdr = t;
    debug(33, 3) ("parseHttpRequest: req_hdr = {%s}\n", req_hdr);

    /* Check if headers are received */
    header_sz = headersEnd(req_hdr, conn->in.offset - (req_hdr - inbuf));
    if (0 == header_sz) {
	xfree(inbuf);
	*status = 0;
	return NULL;
    }
    end = req_hdr + header_sz;
    debug(33, 3) ("parseHttpRequest: end = {%s}\n", end);

    if (end <= req_hdr) {
	/* Invalid request */
	debug(33, 3) ("parseHttpRequest: No request headers?\n");
	http = xcalloc(1, sizeof(clientHttpRequest));
	cbdataAdd(http, MEM_NONE);
	http->conn = conn;
	http->start = current_time;
	http->req_sz = conn->in.offset;
	http->uri = xstrdup("error:no-request-headers");
	http->log_uri = xstrdup("error:no-request-headers");
	*headers_sz_p = conn->in.offset;
	*headers_p = inbuf;
	*status = -1;
	return http;
    }
    req_sz = end - inbuf;
    debug(33, 3) ("parseHttpRequest: req_sz = %d\n", (int) req_sz);
    assert(req_sz <= conn->in.offset);

    /* Ok, all headers are received */
    http = xcalloc(1, sizeof(clientHttpRequest));
    cbdataAdd(http, MEM_NONE);
    http->http_ver = http_ver;
    http->conn = conn;
    http->start = current_time;
    http->req_sz = req_sz;
    *headers_sz_p = header_sz;
    *headers_p = xmalloc(header_sz + 1);
    xmemcpy(*headers_p, req_hdr, header_sz);
    *(*headers_p + header_sz) = '\0';

    debug(33, 5) ("parseHttpRequest: Request Header is\n%s\n", *headers_p);
    /* temporary kludge to test headers, remove it @?@ @?@ */
    {
	extern void httpHeaderTestParser(const char *hstr);
	httpHeaderTestParser(*headers_p);
    }
    /* end of kludge */

    /* Assign http->uri */
    if ((t = strchr(url, '\n')))	/* remove NL */
	*t = '\0';
    if ((t = strchr(url, '\r')))	/* remove CR */
	*t = '\0';
    if ((t = strchr(url, '#')))	/* remove HTML anchors */
	*t = '\0';

    /* handle internal objects */
    if (*url == '/' && strncmp(url, "/squid-internal/", 16) == 0) {
	/* prepend our name & port */
	url_sz = 7 + strlen(getMyHostname()) + 6 + strlen(url) + 1;
	http->uri = xcalloc(url_sz, 1);
	snprintf(http->uri, url_sz, "http://%s:%d%s",
	    getMyHostname(), Config.Port.http->i, url);
	http->internal = 1;
    }
    /* see if we running in Config2.Accel.on, if so got to convert it to URL */
    else if (Config2.Accel.on && *url == '/') {
	/* prepend the accel prefix */
	if (opt_accel_uses_host && (t = mime_get_header(req_hdr, "Host"))) {
	    /* If a Host: header was specified, use it to build the URL 
	     * instead of the one in the Config file. */
	    /*
	     * XXX Use of the Host: header here opens a potential
	     * security hole.  There are no checks that the Host: value
	     * corresponds to one of your servers.  It might, for example,
	     * refer to www.playboy.com.  The 'dst' and/or 'dst_domain' ACL 
	     * types should be used to prevent httpd-accelerators 
	     * handling requests for non-local servers */
	    strtok(t, " :/;@");
	    url_sz = strlen(url) + 32 + Config.appendDomainLen +
		strlen(t);
	    http->uri = xcalloc(url_sz, 1);
	    snprintf(http->uri, url_sz, "http://%s:%d%s",
		t, (int) Config.Accel.port, url);
	} else if (vhost_mode) {
	    /* Put the local socket IP address as the hostname */
	    url_sz = strlen(url) + 32 + Config.appendDomainLen;
	    http->uri = xcalloc(url_sz, 1);
	    snprintf(http->uri, url_sz, "http://%s:%d%s",
		inet_ntoa(http->conn->me.sin_addr),
		(int) Config.Accel.port,
		url);
	    debug(33, 5) ("VHOST REWRITE: '%s'\n", http->uri);
	} else {
	    url_sz = strlen(Config2.Accel.prefix) + strlen(url) +
		Config.appendDomainLen + 1;
	    http->uri = xcalloc(url_sz, 1);
	    snprintf(http->uri, url_sz, "%s%s", Config2.Accel.prefix, url);
	}
	http->accel = 1;
    } else {
	/* URL may be rewritten later, so make extra room */
	url_sz = strlen(url) + Config.appendDomainLen + 5;
	http->uri = xcalloc(url_sz, 1);
	strcpy(http->uri, url);
	http->accel = 0;
    }
    http->log_uri = xstrdup(http->uri);
    debug(33, 5) ("parseHttpRequest: Complete request received\n");
    if (free_request)
	safe_free(url);
    xfree(inbuf);
    *status = 1;
    return http;
}

static int
clientReadDefer(int fdnotused, void *data)
{
    ConnStateData *conn = data;
    return conn->defer.until > squid_curtime;
}

static void
clientReadRequest(int fd, void *data)
{
    ConnStateData *conn = data;
    int parser_return_code = 0;
    int k;
    request_t *request = NULL;
    int size;
    method_t method;
    clientHttpRequest *http = NULL;
    clientHttpRequest **H = NULL;
    char *headers;
    size_t headers_sz;
    ErrorState *err = NULL;
    fde *F = &fd_table[fd];
    int len = conn->in.size - conn->in.offset - 1;
    debug(33, 4) ("clientReadRequest: FD %d: reading request...\n", fd);
    size = read(fd, conn->in.buf + conn->in.offset, len);
    fd_bytes(fd, size, FD_READ);
    /*
     * Don't reset the timeout value here.  The timeout value will be
     * set to Config.Timeout.request by httpAccept() and
     * clientWriteComplete(), and should apply to the request as a
     * whole, not individual read() calls.  Plus, it breaks our
     * lame half-close detection
     */
    commSetSelect(fd, COMM_SELECT_READ, clientReadRequest, conn, 0);
    if (size == 0) {
	if (conn->chr == NULL) {
	    /* no current or pending requests */
	    comm_close(fd);
	    return;
	}
	/* It might be half-closed, we can't tell */
	debug(33, 5) ("clientReadRequest: FD %d closed?\n", fd);
	EBIT_SET(F->flags, FD_SOCKET_EOF);
	conn->defer.until = squid_curtime + 1;
	conn->defer.n++;
	fd_note(fd, "half-closed");
	return;
    } else if (size < 0) {
	if (!ignoreErrno(errno)) {
	    debug(50, 2) ("clientReadRequest: FD %d: %s\n", fd, xstrerror());
	    comm_close(fd);
	    return;
	} else if (conn->in.offset == 0) {
	    debug(50, 2) ("clientReadRequest: FD %d: no data to process\n", fd);
	    return;
	}
	/* Continue to process previously read data */
	size = 0;
    }
    conn->in.offset += size;
    conn->in.buf[conn->in.offset] = '\0';	/* Terminate the string */
    while (conn->in.offset > 0) {
	int nrequests;
	/* Limit the number of concurrent requests to 2 */
	for (H = &conn->chr, nrequests = 0; *H; H = &(*H)->next, nrequests++);
	if (nrequests >= 2) {
	    debug(33, 2) ("clientReadRequest: FD %d max concurrent requests reached\n", fd);
	    debug(33, 5) ("clientReadRequest: FD %d defering new request until one is done\n", fd);
	    conn->defer.until = squid_curtime + 100;	/* Reset when a request is complete */
	    break;
	}
	/* Process request */
	http = parseHttpRequest(conn,
	    &method,
	    &parser_return_code,
	    &headers,
	    &headers_sz);
	if (http) {
	    assert(http->req_sz > 0);
	    conn->in.offset -= http->req_sz;
	    assert(conn->in.offset >= 0);
	    /*
	     * If we read past the end of this request, move the remaining
	     * data to the beginning
	     */
	    if (conn->in.offset > 0)
		xmemmove(conn->in.buf, conn->in.buf + http->req_sz, conn->in.offset);
	    /* add to the client request queue */
	    for (H = &conn->chr; *H; H = &(*H)->next);
	    *H = http;
	    conn->nrequests++;
	    commSetTimeout(fd, Config.Timeout.lifetime, NULL, NULL);
	    if (parser_return_code < 0) {
		debug(33, 1) ("clientReadRequest: FD %d Invalid Request\n", fd);
		err = errorCon(ERR_INVALID_REQ, HTTP_BAD_REQUEST);
		err->request_hdrs = xstrdup(conn->in.buf);
		http->entry = clientCreateStoreEntry(http, method, 0);
		errorAppendEntry(http->entry, err);
		break;
	    }
	    if ((request = urlParse(method, http->uri)) == NULL) {
		debug(33, 5) ("Invalid URL: %s\n", http->uri);
		err = errorCon(ERR_INVALID_URL, HTTP_BAD_REQUEST);
		err->src_addr = conn->peer.sin_addr;
		err->url = xstrdup(http->uri);
		http->al.http.code = err->http_status;
		http->entry = clientCreateStoreEntry(http, method, 0);
		errorAppendEntry(http->entry, err);
		safe_free(headers);
		break;
	    }
	    safe_free(http->log_uri);
	    http->log_uri = xstrdup(urlCanonicalClean(request));
	    request->client_addr = conn->peer.sin_addr;
	    request->http_ver = http->http_ver;
	    request->headers = headers;
	    request->headers_sz = headers_sz;
	    if (!urlCheckRequest(request)) {
		err = errorCon(ERR_UNSUP_REQ, HTTP_NOT_IMPLEMENTED);
		err->src_addr = conn->peer.sin_addr;
		err->request = requestLink(request);
		http->al.http.code = err->http_status;
		http->entry = clientCreateStoreEntry(http, request->method, 0);
		errorAppendEntry(http->entry, err);
		break;
	    }
	    if (0 == clientCheckContentLength(request)) {
		err = errorCon(ERR_INVALID_REQ, HTTP_LENGTH_REQUIRED);
		err->src_addr = conn->peer.sin_addr;
		err->request = requestLink(request);
		http->al.http.code = err->http_status;
		http->entry = clientCreateStoreEntry(http, request->method, 0);
		errorAppendEntry(http->entry, err);
		break;
	    }
	    http->request = requestLink(request);
	    clientAccessCheck(http);
	    /*
	     * break here for NON-GET because most likely there is a
	     * reqeust body following and we don't want to parse it
	     * as though it was new request
	     */
	    if (request->method != METHOD_GET) {
		if (conn->in.offset) {
		    request->body_sz = conn->in.offset;
		    request->body = xmalloc(request->body_sz);
		    xmemcpy(request->body, conn->in.buf, request->body_sz);
		    conn->in.offset = 0;
		}
		break;
	    }
	    continue;		/* while offset > 0 */
	} else if (parser_return_code == 0) {
	    /*
	     *    Partial request received; reschedule until parseHttpRequest()
	     *    is happy with the input
	     */
	    k = conn->in.size - 1 - conn->in.offset;
	    if (k == 0) {
		if (conn->in.offset >= Config.maxRequestSize) {
		    /* The request is too large to handle */
		    debug(33, 0) ("Request won't fit in buffer.\n");
		    debug(33, 0) ("Config 'request_size'= %d bytes.\n",
			Config.maxRequestSize);
		    debug(33, 0) ("This request = %d bytes.\n",
			(int) conn->in.offset);
		    err = errorCon(ERR_INVALID_REQ, HTTP_REQUEST_ENTITY_TOO_LARGE);
		    http->entry = clientCreateStoreEntry(http, request->method, 0);
		    errorAppendEntry(http->entry, err);
		    return;
		}
		/* Grow the request memory area to accomodate for a large request */
		conn->in.size += REQUEST_BUF_SIZE;
		conn->in.buf = xrealloc(conn->in.buf, conn->in.size);
		/* XXX account conn->in.buf */
		debug(33, 2) ("Handling a large request, offset=%d inbufsize=%d\n",
		    (int) conn->in.offset, conn->in.size);
		k = conn->in.size - 1 - conn->in.offset;
	    }
	    break;
	}
    }
}

/* general lifetime handler for HTTP requests */
static void
requestTimeout(int fd, void *data)
{
    ConnStateData *conn = data;
    ErrorState *err;
    debug(33, 2) ("requestTimeout: FD %d: lifetime is expired.\n", fd);
    if (fd_table[fd].rwstate) {
	/*
	 * Some data has been sent to the client, just close the FD
	 */
	comm_close(fd);
    } else if (conn->nrequests) {
	/*
	 * assume its a persistent connection; just close it
	 */
	comm_close(fd);
    } else {
	/*
	 * Generate an error
	 */
	err = errorCon(ERR_LIFETIME_EXP, HTTP_REQUEST_TIMEOUT);
	err->url = xstrdup("N/A");
	/*
	 * Normally we shouldn't call errorSend() in client_side.c, but
	 * it should be okay in this case.  Presumably if we get here
	 * this is the first request for the connection, and no data
	 * has been written yet
	 */
	assert(conn->chr == NULL);
	errorSend(fd, err);
	/*
	 * if we don't close() here, we still need a timeout handler!
	 */
	commSetTimeout(fd, 30, requestTimeout, conn);
    }
}

int
httpAcceptDefer(int fdnotused, void *notused)
{
    return fdNFree() < RESERVED_FD;
}

/* Handle a new connection on HTTP socket. */
void
httpAccept(int sock, void *notused)
{
    int fd = -1;
    ConnStateData *connState = NULL;
    struct sockaddr_in peer;
    struct sockaddr_in me;
    memset(&peer, '\0', sizeof(struct sockaddr_in));
    memset(&me, '\0', sizeof(struct sockaddr_in));
    commSetSelect(sock, COMM_SELECT_READ, httpAccept, NULL, 0);
    if ((fd = comm_accept(sock, &peer, &me)) < 0) {
	debug(50, 1) ("httpAccept: FD %d: accept failure: %s\n",
	    sock, xstrerror());
	return;
    }
    debug(33, 4) ("httpAccept: FD %d: accepted\n", fd);
    connState = xcalloc(1, sizeof(ConnStateData));
    connState->peer = peer;
    connState->log_addr = peer.sin_addr;
    connState->log_addr.s_addr &= Config.Addrs.client_netmask.s_addr;
    connState->me = me;
    connState->fd = fd;
    connState->ident.fd = -1;
    connState->in.size = REQUEST_BUF_SIZE;
    connState->in.buf = xcalloc(connState->in.size, 1);
    cbdataAdd(connState, MEM_NONE);
    /* XXX account connState->in.buf */
    comm_add_close_handler(fd, connStateFree, connState);
    if (Config.onoff.log_fqdn)
	fqdncache_gethostbyaddr(peer.sin_addr, FQDN_LOOKUP_IF_MISS);
    commSetTimeout(fd, Config.Timeout.request, requestTimeout, connState);
    commSetSelect(fd, COMM_SELECT_READ, clientReadRequest, connState, 0);
    commSetDefer(fd, clientReadDefer, connState);
}

void
AppendUdp(icpUdpData * item)
{
    item->next = NULL;
    if (UdpQueueHead == NULL) {
	UdpQueueHead = item;
	UdpQueueTail = item;
    } else if (UdpQueueTail == UdpQueueHead) {
	UdpQueueTail = item;
	UdpQueueHead->next = item;
    } else {
	UdpQueueTail->next = item;
	UdpQueueTail = item;
    }
}

/* return 1 if the request should be aborted */
static int
CheckQuickAbort2(const clientHttpRequest * http)
{
    long curlen;
    long minlen;
    long expectlen;

    if (!EBIT_TEST(http->request->flags, REQ_CACHABLE))
	return 1;
    if (EBIT_TEST(http->entry->flag, KEY_PRIVATE))
	return 1;
    if (http->entry->mem_obj == NULL)
	return 1;
    expectlen = http->entry->mem_obj->reply->content_length;
    curlen = http->entry->mem_obj->inmem_hi;
    minlen = Config.quickAbort.min;
    if (minlen < 0)
	/* disabled */
	return 0;
    if (curlen > expectlen)
	/* bad content length */
	return 1;
    if ((expectlen - curlen) < minlen)
	/* only little more left */
	return 0;
    if ((expectlen - curlen) > Config.quickAbort.max)
	/* too much left to go */
	return 1;
    if ((curlen / (expectlen / 128U)) > Config.quickAbort.pct)
	/* past point of no return */
	return 0;
    return 1;
}


static void
CheckQuickAbort(clientHttpRequest * http)
{
    StoreEntry *entry = http->entry;
    /* Note, set entry here because http->entry might get changed (for IMS
     * requests) during the storeAbort() call */
    if (entry == NULL)
	return;
    if (storePendingNClients(entry) > 0)
	return;
    if (entry->store_status != STORE_PENDING)
	return;
    if (CheckQuickAbort2(http) == 0)
	return;
    debug(33, 3) ("CheckQuickAbort: ABORTING %s\n", storeUrl(entry));
    storeAbort(entry, 1);
}

#define SENDING_BODY 0
#define SENDING_HDRSONLY 1
static int
clientCheckTransferDone(clientHttpRequest * http)
{
    int sending = SENDING_BODY;
    StoreEntry *entry = http->entry;
    MemObject *mem;
    http_reply *reply;
    int sendlen;
    if (entry == NULL)
	return 0;
    /*
     * Handle STORE_OK and STORE_ABORTED objects.
     * objectLen(entry) will be set proprely.
     */
    if (entry->store_status != STORE_PENDING) {
	if (http->out.offset >= objectLen(entry))
	    return 1;
	else
	    return 0;
    }
    /*
     * Now, handle STORE_PENDING objects
     */
    mem = entry->mem_obj;
    assert(mem != NULL);
    assert(http->request != NULL);
    reply = mem->reply;
    if (reply->hdr_sz == 0)
	return 0;		/* haven't found end of headers yet */
    else if (reply->sline.status == HTTP_OK)
	sending = SENDING_BODY;
    else if (reply->sline.status == HTTP_NO_CONTENT)
	sending = SENDING_HDRSONLY;
    else if (reply->sline.status == HTTP_NOT_MODIFIED)
	sending = SENDING_HDRSONLY;
    else if (reply->sline.status < HTTP_OK)
	sending = SENDING_HDRSONLY;
    else if (http->request->method == METHOD_HEAD)
	sending = SENDING_HDRSONLY;
    else
	sending = SENDING_BODY;
    /*
     * Figure out how much data we are supposed to send.
     * If we are sending a body and we don't have a content-length,
     * then we must wait for the object to become STORE_OK or
     * STORE_ABORTED.
     */
    if (sending == SENDING_HDRSONLY)
	sendlen = reply->hdr_sz;
    else if (reply->content_length < 0)
	return 0;
    else
	sendlen = reply->content_length + reply->hdr_sz;
    /*
     * Now that we have the expected length, did we send it all?
     */
    if (http->out.offset < sendlen)
	return 0;
    else
	return 1;
}

/*
 * This function is designed to serve a fairly specific purpose.
 * Occasionally our vBNS-connected caches can talk to each other, but not
 * the rest of the world.  Here we try to detect frequent failures which
 * make the cache unusable (e.g. DNS lookup and connect() failures).  If
 * the failure:success ratio goes above 1.0 then we go into "hit only"
 * mode where we only return UDP_HIT or UDP_MISS_NOFETCH.  Neighbors
 * will only fetch HITs from us if they are using the ICP protocol.  We
 * stay in this mode for 5 minutes.
 * 
 * Duane W., Sept 16, 1996
 */

static void
checkFailureRatio(err_type etype, hier_code hcode)
{
    static double magic_factor = 100.0;
    double n_good;
    double n_bad;
    if (hcode == HIER_NONE)
	return;
    n_good = magic_factor / (1.0 + request_failure_ratio);
    n_bad = magic_factor - n_good;
    switch (etype) {
    case ERR_DNS_FAIL:
    case ERR_CONNECT_FAIL:
    case ERR_READ_ERROR:
	n_bad++;
	break;
    default:
	n_good++;
    }
    request_failure_ratio = n_bad / n_good;
    if (hit_only_mode_until > squid_curtime)
	return;
    if (request_failure_ratio < 1.0)
	return;
    debug(33, 0) ("Failure Ratio at %4.2f\n", request_failure_ratio);
    debug(33, 0) ("Going into hit-only-mode for %d minutes...\n",
	FAILURE_MODE_TIME / 60);
    hit_only_mode_until = squid_curtime + FAILURE_MODE_TIME;
    request_failure_ratio = 0.8;	/* reset to something less than 1.0 */
}

void
clientHttpConnectionsOpen(void)
{
    ushortlist *u;
    int fd;
    for (u = Config.Port.http; u; u = u->next) {
	enter_suid();
	fd = comm_open(SOCK_STREAM,
	    0,
	    Config.Addrs.tcp_incoming,
	    u->i,
	    COMM_NONBLOCKING,
	    "HTTP Socket");
	leave_suid();
	if (fd < 0)
	    continue;
	comm_listen(fd);
	commSetSelect(fd, COMM_SELECT_READ, httpAccept, NULL, 0);
	commSetDefer(fd, httpAcceptDefer, NULL);
	debug(1, 1) ("Accepting HTTP connections on port %d, FD %d.\n",
	    (int) u->i, fd);
	HttpSockets[NHttpSockets++] = fd;
    }
    if (NHttpSockets < 1)
	fatal("Cannot open HTTP Port");
}

int
handleConnectionHeader(int flag, char *where, char *what)
{
    char *t, *p, *wh;
    int i;
    LOCAL_ARRAY(char, mbuf, 256);

    if (flag) {			/* lookup mode */
	if (where[0] == '\0' || what[0] == '\0')
	    return 0;
	p = xstrdup(what);
	t = strtok(p, ":");
	if (t == NULL)
	    return 0;
	debug(20, 3) ("handleConnectionHeader: %s\n AND %s (%p)\n", where, t, p);
	i = strstr(where, t) ? 1 : 0;
	xfree(p);
	return (i);
    }
    where[0] = '\0';
    wh = xstrdup(what);
    t = strtok(wh, ",");
    while (t != NULL) {

#ifdef BE_PARANOID
	static char no_conn[] = "Expires:Host:Content-length:Content-type:";

	if (handleConnectionHeader(1, no_conn, t)) {
	    debug(1, 1) ("handleConnectionHeader: problematic header %s\n", t);
	    t = strtok(NULL, ",\n");
	    continue;
	}
#endif
	if ((p = strchr(t, ':')))
	    xstrncpy(mbuf, t, p - t + 1);
	else
	    snprintf(mbuf, 256, "%s:", t);
	strcat(where, mbuf);
	t = strtok(NULL, ",\n");
    }
    debug(20, 3) ("handleConnectionHeader: we have %s\n", where);
    xfree(wh);
    return 1;
}

void
clientHttpConnectionsClose(void)
{
    int i;
    for (i = 0; i < NHttpSockets; i++) {
	if (HttpSockets[i] >= 0) {
	    debug(1, 1) ("FD %d Closing HTTP connection\n", HttpSockets[i]);
	    comm_close(HttpSockets[i]);
	    HttpSockets[i] = -1;
	}
    }
    NHttpSockets = 0;
}

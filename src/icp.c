
/*
 * $Id$
 *
 * DEBUG: section 12    Client Handling
 * AUTHOR: Harvest Derived
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

/*
 * Copyright (c) 1994, 1995.  All rights reserved.
 *  
 *   The Harvest software was developed by the Internet Research Task
 *   Force Research Group on Resource Discovery (IRTF-RD):
 *  
 *         Mic Bowman of Transarc Corporation.
 *         Peter Danzig of the University of Southern California.
 *         Darren R. Hardy of the University of Colorado at Boulder.
 *         Udi Manber of the University of Arizona.
 *         Michael F. Schwartz of the University of Colorado at Boulder.
 *         Duane Wessels of the University of Colorado at Boulder.
 *  
 *   This copyright notice applies to software in the Harvest
 *   ``src/'' directory only.  Users should consult the individual
 *   copyright notices in the ``components/'' subdirectories for
 *   copyright information about other software bundled with the
 *   Harvest source code distribution.
 *  
 * TERMS OF USE
 *   
 *   The Harvest software may be used and re-distributed without
 *   charge, provided that the software origin and research team are
 *   cited in any use of the system.  Most commonly this is
 *   accomplished by including a link to the Harvest Home Page
 *   (http://harvest.cs.colorado.edu/) from the query page of any
 *   Broker you deploy, as well as in the query result pages.  These
 *   links are generated automatically by the standard Broker
 *   software distribution.
 *   
 *   The Harvest software is provided ``as is'', without express or
 *   implied warranty, and with no support nor obligation to assist
 *   in its use, correction, modification or enhancement.  We assume
 *   no liability with respect to the infringement of copyrights,
 *   trade secrets, or any patents, and are not responsible for
 *   consequential damages.  Proper use of the Harvest software is
 *   entirely the responsibility of the user.
 *  
 * DERIVATIVE WORKS
 *  
 *   Users may make derivative works from the Harvest software, subject 
 *   to the following constraints:
 *  
 *     - You must include the above copyright notice and these 
 *       accompanying paragraphs in all forms of derivative works, 
 *       and any documentation and other materials related to such 
 *       distribution and use acknowledge that the software was 
 *       developed at the above institutions.
 *  
 *     - You must notify IRTF-RD regarding your distribution of 
 *       the derivative work.
 *  
 *     - You must clearly notify users that your are distributing 
 *       a modified version and not the original Harvest software.
 *  
 *     - Any derivative product is also subject to these copyright 
 *       and use restrictions.
 *  
 *   Note that the Harvest software is NOT in the public domain.  We
 *   retain copyright, as specified above.
 *  
 * HISTORY OF FREE SOFTWARE STATUS
 *  
 *   Originally we required sites to license the software in cases
 *   where they were going to build commercial products/services
 *   around Harvest.  In June 1995 we changed this policy.  We now
 *   allow people to use the core Harvest software (the code found in
 *   the Harvest ``src/'' directory) for free.  We made this change
 *   in the interest of encouraging the widest possible deployment of
 *   the technology.  The Harvest software is really a reference
 *   implementation of a set of protocols and formats, some of which
 *   we intend to standardize.  We encourage commercial
 *   re-implementations of code complying to this set of standards.  
 */

#include "squid.h"

int neighbors_do_private_keys = 1;

char *log_tags[] =
{
    "NONE",
    "TCP_HIT",
    "TCP_MISS",
    "TCP_REFRESH_HIT",
    "TCP_REF_FAIL_HIT",
    "TCP_REFRESH_MISS",
    "TCP_CLIENT_REFRESH",
    "TCP_IMS_HIT",
    "TCP_IMS_MISS",
    "TCP_SWAPFAIL",
    "TCP_DENIED",
    "UDP_HIT",
    "UDP_HIT_OBJ",
    "UDP_MISS",
    "UDP_DENIED",
    "UDP_INVALID",
    "UDP_RELOADING",
    "ERR_READ_TIMEOUT",
    "ERR_LIFETIME_EXP",
    "ERR_NO_CLIENTS_BIG_OBJ",
    "ERR_READ_ERROR",
    "ERR_CLIENT_ABORT",
    "ERR_CONNECT_FAIL",
    "ERR_INVALID_REQ",
    "ERR_UNSUP_REQ",
    "ERR_INVALID_URL",
    "ERR_NO_FDS",
    "ERR_DNS_FAIL",
    "ERR_NOT_IMPLEMENTED",
    "ERR_CANNOT_FETCH",
    "ERR_NO_RELAY",
    "ERR_DISK_IO",
    "ERR_ZERO_SIZE_OBJECT",
    "ERR_FTP_DISABLED",
    "ERR_PROXY_DENIED"
};

#if DELAY_HACK
int _delay_fetch;
#endif

static icpUdpData *UdpQueueHead = NULL;
static icpUdpData *UdpQueueTail = NULL;
#define ICP_SENDMOREDATA_BUF SM_PAGE_SIZE

typedef struct {
    int fd;
    struct sockaddr_in to;
    StoreEntry *entry;
    icp_common_t header;
    int pad;
} icpHitObjStateData;

/* Local functions */
static char *icpConstruct304reply _PARAMS((struct _http_reply *));
static int CheckQuickAbort2 _PARAMS((const icpStateData *));
static int icpProcessMISS _PARAMS((int, icpStateData *));
static void CheckQuickAbort _PARAMS((icpStateData *));
static void checkFailureRatio _PARAMS((log_type, hier_code));
static void clientWriteComplete _PARAMS((int, char *, int, int, void *icpState));
static void icpHandleStoreIMS _PARAMS((int, StoreEntry *, void *));
static void icpHandleIMSComplete _PARAMS((int, char *, int, int, void *icpState));
static void icpHitObjHandler _PARAMS((int, void *));
static void icpLogIcp _PARAMS((icpUdpData *));
static void icpHandleIcpV2 _PARAMS((int, struct sockaddr_in, char *, int));
static void icpHandleIcpV3 _PARAMS((int, struct sockaddr_in, char *, int));
static void icpSendERRORComplete _PARAMS((int, char *, int, int, void *));
static void icpHandleAbort _PARAMS((int fd, void *));
static int icpCheckUdpHit _PARAMS((StoreEntry *, request_t * request));
static int icpCheckUdpHitObj _PARAMS((StoreEntry * e, request_t * r, icp_common_t * h, int len));
static void icpStateFree _PARAMS((int fd, void *data));
static int icpCheckTransferDone _PARAMS((icpStateData *));
static int icpReadDataDone _PARAMS((int fd, char *buf, int len, int err, void *data));

/*
 * This function is designed to serve a fairly specific purpose.
 * Occasionally our vBNS-connected caches can talk to each other, but not
 * the rest of the world.  Here we try to detect frequent failures which
 * make the cache unusable (e.g. DNS lookup and connect() failures).  If
 * the failure:success ratio goes above 1.0 then we go into "hit only"
 * mode where we only return UDP_HIT or UDP_RELOADING.  (UDP_RELOADING
 * isn't quite the appropriate term but it gets the job done).  Neighbors
 * will only fetch HITs from us if they are using the ICP protocol.  We
 * stay in this mode for 5 minutes.
 * 
 * Duane W., Sept 16, 1996
 */

#define FAILURE_MODE_TIME 300
static time_t hit_only_mode_until = 0;

static void
checkFailureRatio(log_type rcode, hier_code hcode)
{
    static double fail_ratio = 0.0;
    static double magic_factor = 100;
    double n_good;
    double n_bad;
    if (hcode == HIER_NONE)
	return;
    n_good = magic_factor / (1.0 + fail_ratio);
    n_bad = magic_factor - n_good;
    switch (rcode) {
    case ERR_DNS_FAIL:
    case ERR_CONNECT_FAIL:
    case ERR_READ_ERROR:
	n_bad++;
	break;
    default:
	n_good++;
    }
    fail_ratio = n_bad / n_good;
    if (hit_only_mode_until > squid_curtime)
	return;
    if (fail_ratio < 1.0)
	return;
    debug(12, 0, "Failure Ratio at %4.2f\n", fail_ratio);
    debug(12, 0, "Going into hit-only-mode for %d minutes...\n",
	FAILURE_MODE_TIME / 60);
    hit_only_mode_until = squid_curtime + FAILURE_MODE_TIME;
    fail_ratio = 0.8;		/* reset to something less than 1.0 */
}

/* This is a handler normally called by comm_close() */
static void
icpStateFree(int fd, void *data)
{
    icpStateData *icpState = data;
    int http_code = 0;
    int elapsed_msec;
    struct _hierarchyLogData *hierData = NULL;
    const char *content_type = NULL;

    if (!icpState)
	return;
    if (icpState->log_type < LOG_TAG_NONE || icpState->log_type > ERR_MAX)
	fatal_dump("icpStateFree: icpState->log_type out of range.");
    if (icpState->swapin_fd > -1)
	file_close(icpState->swapin_fd);
    if (icpState->entry) {
	if (icpState->entry->mem_obj) {
	    http_code = icpState->entry->mem_obj->reply->code;
	    content_type = icpState->entry->mem_obj->reply->content_type;
	}
    } else {
	http_code = icpState->http_code;
    }
    elapsed_msec = tvSubMsec(icpState->start, current_time);
    if (icpState->request)
	hierData = &icpState->request->hierarchy;
    if (icpState->out.size || icpState->log_type) {
	HTTPCacheInfo->log_append(HTTPCacheInfo,
	    icpState->url,
	    icpState->log_addr,
	    icpState->out.size,
	    log_tags[icpState->log_type],
	    RequestMethodStr[icpState->method],
	    http_code,
	    elapsed_msec,
	    icpState->ident.ident,
	    hierData,
#if LOG_FULL_HEADERS
	    icpState->request_hdr,
	    icpState->reply_hdr,
#endif /* LOG_FULL_HEADERS */
	    content_type);
	HTTPCacheInfo->proto_count(HTTPCacheInfo,
	    icpState->request ? icpState->request->protocol : PROTO_NONE,
	    icpState->log_type);
	clientdbUpdate(icpState->peer.sin_addr,
	    icpState->log_type,
	    ntohs(icpState->me.sin_port));
    }
    if (icpState->ident.fd > -1)
	comm_close(icpState->ident.fd);
    checkFailureRatio(icpState->log_type,
	hierData ? hierData->code : HIER_NONE);
    put_free_8k_page(icpState->out.buf);
    safe_free(icpState->in.buf);
    meta_data.misc -= icpState->in.size;
    safe_free(icpState->url);
    safe_free(icpState->request_hdr);
#if LOG_FULL_HEADERS
    safe_free(icpState->reply_hdr);
#endif /* LOG_FULL_HEADERS */
    if (icpState->entry) {
	storeUnregister(icpState->entry, fd);
	storeUnlockObject(icpState->entry);
	icpState->entry = NULL;
    }
    if (icpState->old_entry) {
	storeUnlockObject(icpState->old_entry);
	icpState->old_entry = NULL;
    }
    requestUnlink(icpState->request);
    if (icpState->aclChecklist) {
	debug(12, 0, "icpStateFree: still have aclChecklist!\n");
	requestUnlink(icpState->aclChecklist->request);
	safe_free(icpState->aclChecklist);
    }
    safe_free(icpState);
    return;
}

void
icpParseRequestHeaders(icpStateData * icpState)
{
    char *request_hdr = icpState->request_hdr;
    char *t = NULL;
    request_t *request = icpState->request;
    request->ims = -2;
    request->imslen = -1;
    if ((t = mime_get_header(request_hdr, "If-Modified-Since"))) {
	BIT_SET(request->flags, REQ_IMS);
	debug(12, 5, "icpParseRequestHeaders: setting REQ_IMS\n");
	request->ims = parse_rfc1123(t);
	while ((t = strchr(t, ';'))) {
	    for (t++; isspace(*t); t++);
	    if (strncasecmp(t, "length=", 7) == 0)
		request->imslen = atoi(t + 7);
	}
    }
    if ((t = mime_get_header(request_hdr, "Pragma"))) {
	if (!strcasecmp(t, "no-cache"))
	    BIT_SET(request->flags, REQ_NOCACHE);
    }
    if (mime_get_header(request_hdr, "Authorization"))
	BIT_SET(request->flags, REQ_AUTH);
#if TRY_KEEPALIVE_SUPPORT
    if ((t = mime_get_header(request_hdr, "Proxy-Connection")))
	if (!strcasecmp(t, "Keep-Alive"))
	    BIT_SET(request->flags, REQ_PROXY_KEEPALIVE);
#endif
    if ((t = mime_get_header(request_hdr, "Via")))
	if (strstr(t, ThisCache)) {
	    if (!icpState->accel) {
		debug(12, 1, "WARNING: Forwarding loop detected for '%s'\n",
		    icpState->url);
		debug(12, 1, "--> %s\n", t);
	    }
	    BIT_SET(request->flags, REQ_LOOPDETECT);
	}
#if USE_USERAGENT_LOG
    if ((t = mime_get_header(request_hdr, "User-Agent")))
	logUserAgent(fqdnFromAddr(icpState->peer.sin_addr), t);
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
}

static int
icpCachable(icpStateData * icpState)
{
    const char *request = icpState->url;
    request_t *req = icpState->request;
    method_t method = req->method;
    const wordlist *p;
    if (BIT_TEST(icpState->request->flags, REQ_AUTH))
	return 0;
    for (p = Config.cache_stoplist; p; p = p->next) {
	if (strstr(request, p->key))
	    return 0;
    }
    if (Config.cache_stop_relist)
	if (aclMatchRegex(Config.cache_stop_relist, request))
	    return 0;
    if (req->protocol == PROTO_HTTP)
	return httpCachable(request, method);
    /* FTP is always cachable */
    if (req->protocol == PROTO_GOPHER)
	return gopherCachable(request);
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
icpHierarchical(icpStateData * icpState)
{
    const char *url = icpState->url;
    request_t *request = icpState->request;
    method_t method = request->method;
    const wordlist *p = NULL;
    /* IMS needs a private key, so we can use the hierarchy for IMS only
     * if our neighbors support private keys */
    if (BIT_TEST(request->flags, REQ_IMS) && !neighbors_do_private_keys)
	return 0;
    if (BIT_TEST(request->flags, REQ_AUTH))
	return 0;
    if (method == METHOD_TRACE)
	return 1;
    if (method != METHOD_GET)
	return 0;
    /* scan hierarchy_stoplist */
    for (p = Config.hierarchy_stoplist; p; p = p->next)
	if (strstr(url, p->key))
	    return 0;
    if (BIT_TEST(request->flags, REQ_LOOPDETECT))
	return 0;
    if (request->protocol == PROTO_HTTP)
	return httpCachable(url, method);
    if (request->protocol == PROTO_GOPHER)
	return gopherCachable(url);
    if (request->protocol == PROTO_WAIS)
	return 0;
    if (request->protocol == PROTO_CACHEOBJ)
	return 0;
    return 1;
}

static void
icpSendERRORComplete(int fd, char *buf, int size, int errflag, void *data)
{
    icpStateData *icpState = data;
    debug(12, 4, "icpSendERRORComplete: FD %d: sz %d: err %d.\n",
	fd, size, errflag);
    icpState->out.size += size;
    comm_close(fd);
}

/* Send ERROR message. */
void
icpSendERROR(int fd,
    log_type errorCode,
    const char *text,
    icpStateData * icpState,
    int httpCode)
{
    int buf_len = 0;
    char *buf = NULL;
    icpState->log_type = errorCode;
    icpState->http_code = httpCode;
    if (icpState->entry && icpState->entry->mem_obj) {
	if (icpState->out.size > 0) {
	    comm_close(fd);
	    return;
	}
    }
    buf_len = strlen(text);
    buf_len = buf_len > 4095 ? 4095 : buf_len;
    buf = get_free_4k_page();
    xstrncpy(buf, text, 4096);
    comm_write(fd,
	buf,
	buf_len,
	30,
	icpSendERRORComplete,
	(void *) icpState,
	put_free_4k_page);
}

#if LOG_FULL_HEADERS
/*
 * Scan beginning of swap object being returned and hope that it contains
 * a complete MIME header for use in reply header logging (log_append).
 */
void
icp_maybe_remember_reply_hdr(icpStateData * icpState)
{
    char *mime;
    char *end;

    if (icpState && icpState->entry && icpState->entry->mem_obj
	&& icpState->entry->mem_obj->data && icpState->entry->mem_obj->data->head
	&& (mime = icpState->entry->mem_obj->data->head->data) != NULL
	&& (end = mime_headers_end(mime)) != NULL) {
	int mime_len = end - mime;
	char *buf = xcalloc(mime_len + 1, 1);

	xstrncpy(buf, mime, mime_len);
	icpState->reply_hdr = buf;
	debug(12, 5, "icp_maybe_remember_reply_hdr: ->\n%s<-\n", buf);
    } else {
	icpState->reply_hdr = 0;
    }
}
#endif /* LOG_FULL_HEADERS */

static int
icpReadDataDone(int fd, char *buf, int len, int err, void *data)
{
    icpStateData *icpState = data;
    StoreEntry *entry = icpState->entry;
    MemObject *mem = entry->mem_obj;
    char *p = NULL;
    debug(12, 3, "icpReadDataDone: FD %d, len=%d, err=%d, '%s'\n",
	fd, len, err, entry->key);
    if (len == 0 && err == DISK_EOF) {
	comm_close(icpState->fd);
	return COMM_OK;
    }
    if (icpState->out.offset == 0 && entry->object_len > 0)
	if (mem->reply->code == 0)
	    httpParseReplyHeaders(buf, mem->reply);
    icpState->out.offset += len;
    if (icpState->request->method == METHOD_HEAD) {
	if ((p = mime_headers_end(buf))) {
	    *p = '\0';
	    len = p - buf;
	    icpState->out.size = entry->object_len;	/* force end */
	}
    }
    comm_write(icpState->fd,
	icpState->out.buf,
	len,
	30,
	clientWriteComplete,
	(void *) icpState,
	NULL);
    return COMM_OK;
}

void
icpSendMoreData(int fd, void *data)
{
    icpStateData *icpState = data;
    StoreEntry *entry = icpState->entry;
    debug(12, 3, "icpSendMoreData: '%s', object_len=%d, offset=%d\n",
	entry->key,
	entry->object_len,
	icpState->out.offset);
    if (entry->store_status == STORE_ABORTED) {
	debug(12, 3, "icpSendMoreData: abort_code=%d url='%s'\n",
	    entry->mem_obj->abort_code, entry->url);
	icpSendERROR(fd,
	    entry->mem_obj->abort_code,
	    entry->mem_obj->e_abort_msg,
	    icpState,
	    400);
	return;
    }
    file_read(icpState->swapin_fd,
	icpState->out.buf,
	DISK_PAGE_SIZE,
	icpState->out.offset,
	icpReadDataDone,
	(void *) icpState);
}

static void
clientWriteComplete(int fd, char *buf, int size, int errflag, void *data)
{
    icpStateData *icpState = data;
    StoreEntry *entry = NULL;

    entry = icpState->entry;
    icpState->out.size += size;
    debug(12, 5, "clientWriteComplete: FD %d, sz %d, err %d, off %d, len %d\n",
	fd, size, errflag, icpState->out.size, entry->object_len);
    if (errflag) {
	CheckQuickAbort(icpState);
	/* Log the number of bytes that we managed to read */
	HTTPCacheInfo->proto_touchobject(HTTPCacheInfo,
	    urlParseProtocol(entry->url),
	    icpState->out.size);
	comm_close(fd);
    } else if (icpCheckTransferDone(icpState)) {
	/* We're finished case */
	HTTPCacheInfo->proto_touchobject(HTTPCacheInfo,
	    icpState->request->protocol,
	    icpState->out.size);
	if (BIT_TEST(icpState->request->flags, REQ_PROXY_KEEPALIVE)) {
	    commCallCloseHandlers(fd);
	    commSetSelect(fd,
		COMM_SELECT_READ,
		NULL,
		NULL,
		0);
	    icpDetectNewRequest(fd);
	} else {
	    comm_close(fd);
	}
    } else {
	storeRegister(icpState->entry, fd, icpSendMoreData, (void *) icpState, icpState->out.offset);
    }
}

static int
icpGetHeadersForIMS(int fd, icpStateData * icpState)
{
    StoreEntry *entry = icpState->entry;
    MemObject *mem = entry->mem_obj;
    int max_len = 8191 - icpState->out.offset;
    char *reply = NULL;
    if (max_len <= 0) {
	debug(12, 1, "icpGetHeadersForIMS: To much headers '%s'\n",
	    entry->key ? entry->key : entry->url);
	icpState->out.offset = 0;
	return icpProcessMISS(fd, icpState);
    }
    if (mem->reply->code == 0) {
	/* All headers are not yet available, wait for more data */
	storeRegister(entry, fd, icpSendMoreData, (void *) icpState, icpState->out.offset);
	return COMM_OK;
    }
    /* All headers are available, check if object is modified or not */
    /* Restart the object from the beginning */
    icpState->out.offset = 0;
    /* Only objects with statuscode==200 can be "Not modified" */
    /* XXX: Should we ignore this? */
    if (mem->reply->code != 200) {
	debug(12, 4, "icpGetHeadersForIMS: Reply code %d!=200\n",
	    mem->reply->code);
	return icpProcessMISS(fd, icpState);
    }
    icpState->log_type = LOG_TCP_IMS_HIT;
    entry->refcount++;
    if (modifiedSince(entry, icpState->request)) {
	icpSendMoreData(fd, icpState);
	return COMM_OK;
    }
    debug(12, 4, "icpGetHeadersForIMS: Not modified '%s'\n", entry->url);
    reply = icpConstruct304reply(mem->reply);
    comm_write(fd,
	xstrdup(reply),
	strlen(reply),
	30,
	icpHandleIMSComplete,
	icpState,
	xfree);
    return COMM_OK;
}

static void
icpHandleStoreIMS(int fd, StoreEntry * entry, void *data)
{
    icpGetHeadersForIMS(fd, data);
}

static void
icpHandleIMSComplete(int fd, char *buf_unused, int size, int errflag, void *data)
{
    icpStateData *icpState = data;
    StoreEntry *entry = icpState->entry;
    debug(12, 5, "icpHandleIMSComplete: Not Modified sent '%s'\n", entry->url);
    HTTPCacheInfo->proto_touchobject(HTTPCacheInfo,
	icpState->request->protocol,
	size);
    /* Set up everything for the logging */
    storeUnlockObject(entry);
    icpState->entry = NULL;
    icpState->out.offset += size;
    icpState->out.size += size;
    icpState->http_code = 304;
    comm_close(fd);
}

/*
 * Below, we check whether the object is a hit or a miss.  If it's a hit,
 * we check whether the object is still valid or whether it is a MISS_TTL.
 */
void
icpProcessRequest(int fd, icpStateData * icpState)
{
    char *url = icpState->url;
    const char *pubkey = NULL;
    StoreEntry *entry = NULL;
    request_t *request = icpState->request;
    char *reply;

    debug(12, 4, "icpProcessRequest: %s '%s'\n",
	RequestMethodStr[icpState->method],
	url);
    if (icpState->method == METHOD_CONNECT) {
	icpState->log_type = LOG_TCP_MISS;
	sslStart(fd,
	    url,
	    icpState->request,
	    icpState->request_hdr,
	    &icpState->out.size);
	return;
    } else if (request->method == METHOD_TRACE) {
	if (request->max_forwards == 0) {
	    reply = clientConstructTraceEcho(icpState);
	    comm_write(fd,
		xstrdup(reply),
		strlen(reply),
		30,
		icpSendERRORComplete,
		icpState,
		xfree);
	    return;
	}
	/* yes, continue */
    } else if (icpState->method != METHOD_GET) {
	icpState->log_type = LOG_TCP_MISS;
	passStart(fd,
	    url,
	    icpState->request,
	    icpState->request_hdr,
	    icpState->req_hdr_sz,
	    &icpState->out.size);
	return;
    }
    if (icpCachable(icpState))
	BIT_SET(request->flags, REQ_CACHABLE);
    if (icpHierarchical(icpState))
	BIT_SET(request->flags, REQ_HIERARCHICAL);

    debug(12, 5, "icpProcessRequest: REQ_NOCACHE = %s\n",
	BIT_TEST(request->flags, REQ_NOCACHE) ? "SET" : "NOT SET");
    debug(12, 5, "icpProcessRequest: REQ_CACHABLE = %s\n",
	BIT_TEST(request->flags, REQ_CACHABLE) ? "SET" : "NOT SET");
    debug(12, 5, "icpProcessRequest: REQ_HIERARCHICAL = %s\n",
	BIT_TEST(request->flags, REQ_HIERARCHICAL) ? "SET" : "NOT SET");

    /* NOTE on HEAD requests: We currently don't cache HEAD reqeusts
     * at all, so look for the corresponding GET object, or just go
     * directly. The only way to get a TCP_HIT on a HEAD reqeust is
     * if someone already did a GET.  Maybe we should turn HEAD
     * misses into full GET's?  */
    if (icpState->method == METHOD_HEAD) {
	pubkey = storeGeneratePublicKey(icpState->url, METHOD_GET);
    } else
	pubkey = storeGeneratePublicKey(icpState->url, icpState->method);

    if ((entry = storeGet(pubkey)) == NULL) {
	/* this object isn't in the cache */
	icpState->log_type = LOG_TCP_MISS;
    } else if (!storeEntryValidToSend(entry)) {
	icpState->log_type = LOG_TCP_MISS;
	storeRelease(entry);
	entry = NULL;
    } else if (BIT_TEST(request->flags, REQ_NOCACHE)) {
	/* IMS+NOCACHE should not eject valid object */
	if (!BIT_TEST(request->flags, REQ_IMS))
	    storeRelease(entry);
	ipcacheReleaseInvalid(icpState->request->host);
	entry = NULL;
	icpState->log_type = LOG_TCP_CLIENT_REFRESH;
    } else if (refreshCheck(entry, request, 0)) {
	/* The object is in the cache, but it needs to be validated.  Use
	 * LOG_TCP_REFRESH_MISS for the time being, maybe change it to
	 * _HIT later in icpHandleIMSReply() */
	if (request->protocol == PROTO_HTTP)
	    icpState->log_type = LOG_TCP_REFRESH_MISS;
	else
	    icpState->log_type = LOG_TCP_MISS;	/* XXX zoinks */
    } else if (BIT_TEST(request->flags, REQ_IMS)) {
	/* User-initiated IMS request for something we think is valid */
	icpState->log_type = LOG_TCP_IMS_MISS;
    } else {
	icpState->log_type = LOG_TCP_HIT;
    }

    if (entry)
	icpState->swapin_fd = storeOpenSwapFileRead(entry);
    if (entry && icpState->swapin_fd < 0) {
	storeRelease(entry);
	entry = NULL;
	icpState->log_type = LOG_TCP_SWAPIN_FAIL;
    }
    if (entry) {
	storeLockObject(entry);
	storeClientListAdd(entry, fd, 0);
    }
    icpState->entry = entry;	/* Save a reference to the object */
    icpState->out.size = 0;
    icpState->out.offset = 0;

    debug(12, 4, "icpProcessRequest: %s for '%s'\n",
	log_tags[icpState->log_type],
	icpState->url);

    switch (icpState->log_type) {
    case LOG_TCP_HIT:
	entry->refcount++;	/* HIT CASE */
	icpSendMoreData(fd, icpState);
	break;
    case LOG_TCP_IMS_MISS:
	icpGetHeadersForIMS(fd, icpState);
	break;
    case LOG_TCP_REFRESH_MISS:
	icpProcessExpired(fd, icpState);
	break;
    default:
	icpProcessMISS(fd, icpState);
	break;
    }
}


/*
 * Prepare to fetch the object as it's a cache miss of some kind.
 */
static int
icpProcessMISS(int fd, icpStateData * icpState)
{
    char *url = icpState->url;
    char *request_hdr = icpState->request_hdr;
    StoreEntry *entry = NULL;
    aclCheck_t ch;
    int answer;
    char *buf;

    debug(12, 4, "icpProcessMISS: '%s %s'\n",
	RequestMethodStr[icpState->method], url);
    debug(12, 10, "icpProcessMISS: request_hdr:\n%s\n", request_hdr);

    /* Check if this host is allowed to fetch MISSES from us */
    memset((char *) &ch, '\0', sizeof(aclCheck_t));
    ch.src_addr = icpState->peer.sin_addr;
    ch.request = requestLink(icpState->request);
    answer = aclCheck(MISSAccessList, &ch);
    requestUnlink(ch.request);
    if (answer == 0) {
	icpState->http_code = 400;
	buf = access_denied_msg(icpState->http_code,
	    icpState->method,
	    icpState->url,
	    fd_table[fd].ipaddr);
	icpSendERROR(fd, LOG_TCP_DENIED, buf, icpState, icpState->http_code);
	return 0;
    }
    /* Get rid of any references to a StoreEntry (if any) */
    if (icpState->entry) {
	storeUnregister(icpState->entry, fd);
	storeUnlockObject(icpState->entry);
	icpState->entry = NULL;
    }
    entry = storeCreateEntry(url,
	request_hdr,
	icpState->req_hdr_sz,
	icpState->request->flags,
	icpState->method);
    /* NOTE, don't call storeLockObject(), storeCreateEntry() does it */
    storeClientListAdd(entry, fd, 0);
    icpState->swapin_fd = storeOpenSwapFileRead(entry);
    if (icpState->swapin_fd < 0)
	fatal_dump("Swapfile open failed");

    entry->refcount++;		/* MISS CASE */
    icpState->entry = entry;
    icpState->out.offset = 0;
    /* Register with storage manager to receive updates when data comes in. */
    storeRegister(entry, fd, icpSendMoreData, (void *) icpState, icpState->out.offset);
#if DELAY_HACK
    ch.src_addr = icpState->peer.sin_addr;
    ch.request = icpState->request;
    _delay_fetch = 0;
    if (aclCheck(DelayAccessList, &ch))
	_delay_fetch = 1;
#endif
    return (protoDispatch(fd, url, icpState->entry, icpState->request));
}

static void
icpLogIcp(icpUdpData * queue)
{
    icp_common_t *header = (icp_common_t *) (void *) queue->msg;
    char *url = (char *) header + sizeof(icp_common_t);
    ICPCacheInfo->proto_touchobject(ICPCacheInfo,
	queue->proto,
	queue->len);
    ICPCacheInfo->proto_count(ICPCacheInfo,
	queue->proto,
	queue->logcode);
    clientdbUpdate(queue->address.sin_addr,
	queue->logcode,
	CACHE_ICP_PORT);
    if (!Config.Options.log_udp)
	return;
    HTTPCacheInfo->log_append(HTTPCacheInfo,
	url,
	queue->address.sin_addr,
	queue->len,
	log_tags[queue->logcode],
	IcpOpcodeStr[ICP_OP_QUERY],
	0,
	tvSubMsec(queue->start, current_time),
	NULL,			/* ident */
	NULL,			/* hierarchy data */
#if LOG_FULL_HEADERS
	NULL,			/* request header */
	NULL,			/* reply header */
#endif /* LOG_FULL_HEADERS */
	NULL);			/* content-type */
}

int
icpUdpReply(int fd, icpUdpData * queue)
{
    int result = COMM_OK;
    int x;
    /* Disable handler, in case of errors. */
    commSetSelect(fd,
	COMM_SELECT_WRITE,
	NULL,
	NULL, 0);
    while ((queue = UdpQueueHead)) {
	debug(12, 5, "icpUdpReply: FD %d sending %d bytes to %s port %d\n",
	    fd,
	    queue->len,
	    inet_ntoa(queue->address.sin_addr),
	    ntohs(queue->address.sin_port));
	x = comm_udp_sendto(fd,
	    &queue->address,
	    sizeof(struct sockaddr_in),
	    queue->msg,
	    queue->len);
	if (x < 0) {
	    if (errno == EWOULDBLOCK || errno == EAGAIN)
		break;		/* don't de-queue */
	    else
		result = COMM_ERROR;
	}
	UdpQueueHead = queue->next;
	if (queue->logcode)
	    icpLogIcp(queue);
	safe_free(queue->msg);
	safe_free(queue);
    }
    /* Reinstate handler if needed */
    if (UdpQueueHead) {
	commSetSelect(fd,
	    COMM_SELECT_WRITE,
	    (PF) icpUdpReply,
	    (void *) UdpQueueHead, 0);
    }
    return result;
}

void *
icpCreateMessage(
    icp_opcode opcode,
    int flags,
    const char *url,
    int reqnum,
    int pad)
{
    char *buf = NULL;
    icp_common_t *headerp = NULL;
    char *urloffset = NULL;
    int buf_len;
    buf_len = sizeof(icp_common_t) + strlen(url) + 1;
    if (opcode == ICP_OP_QUERY)
	buf_len += sizeof(u_num32);
    buf = xcalloc(buf_len, 1);
    headerp = (icp_common_t *) (void *) buf;
    headerp->opcode = opcode;
    headerp->version = ICP_VERSION_CURRENT;
    headerp->length = htons(buf_len);
    headerp->reqnum = htonl(reqnum);
    headerp->flags = htonl(flags);
    headerp->pad = pad;
    headerp->shostid = htonl(theOutICPAddr.s_addr);
    urloffset = buf + sizeof(icp_common_t);
    if (opcode == ICP_OP_QUERY)
	urloffset += sizeof(u_num32);
    memcpy(urloffset, url, strlen(url));
    return buf;
}

void *
icpCreateHitObjMessage(
    icp_opcode opcode,
    int flags,
    const char *url,
    int reqnum,
    int pad,
    StoreEntry * entry)
{

    debug(12, 1, "icpCreateHitObjMessage: NOT WORKING in this version\n");
    return NULL;
#ifdef NOT_WORKING
    char *buf = NULL;
    char *entryoffset = NULL;
    char *urloffset = NULL;
    icp_common_t *headerp = NULL;
    int buf_len;
    u_short data_sz;
    int size;
    MemObject *m = entry->mem_obj;

    buf_len = sizeof(icp_common_t) + strlen(url) + 1 + 2 + entry->object_len;
    if (opcode == ICP_OP_QUERY)
	buf_len += sizeof(u_num32);
    buf = xcalloc(buf_len, 1);
    headerp = (icp_common_t *) (void *) buf;
    headerp->opcode = opcode;
    headerp->version = ICP_VERSION_CURRENT;
    headerp->length = htons(buf_len);
    headerp->reqnum = htonl(reqnum);
    headerp->flags = htonl(flags);
    headerp->pad = pad;
    headerp->shostid = htonl(theOutICPAddr.s_addr);
    urloffset = buf + sizeof(icp_common_t);
    xmemcpy(urloffset, url, strlen(url));
    data_sz = htons((u_short) entry->object_len);
    entryoffset = urloffset + strlen(url) + 1;
    xmemcpy(entryoffset, &data_sz, sizeof(u_short));
    entryoffset += sizeof(u_short);
    size = read(m->swapin_fd, entryoffset, entry->object_len);
    if (size != entry->object_len) {
	debug(12, 1, "icpCreateHitObjMessage: copy failed, wanted %d got %d bytes\n",
	    entry->object_len, size);
	safe_free(buf);
	return NULL;
    }
    return buf;
#endif
}

void
icpUdpSend(int fd,
    const struct sockaddr_in *to,
    icp_common_t * msg,
    log_type logcode,
    protocol_t proto)
{
    icpUdpData *data = xcalloc(1, sizeof(icpUdpData));
    debug(12, 4, "icpUdpSend: Queueing %s for %s\n",
	IcpOpcodeStr[msg->opcode],
	inet_ntoa(to->sin_addr));
    data->address = *to;
    data->msg = msg;
    data->len = (int) ntohs(msg->length);
    data->start = current_time;	/* wrong for HIT_OBJ */
    data->logcode = logcode;
    data->proto = proto;
    AppendUdp(data);
    commSetSelect(fd,
	COMM_SELECT_WRITE,
	(PF) icpUdpReply,
	(void *) UdpQueueHead, 0);
}

static void
icpHitObjHandler(int errflag, void *data)
{
    icpHitObjStateData *icpHitObjState = data;
    StoreEntry *entry = NULL;
    icp_common_t *reply;
    if (data == NULL)
	return;
    entry = icpHitObjState->entry;
    debug(12, 3, "icpHitObjHandler: '%s'\n", entry->url);
    if (errflag) {
	debug(12, 3, "icpHitObjHandler: errflag=%d, aborted!\n", errflag);
    } else {
	reply = icpCreateHitObjMessage(ICP_OP_HIT_OBJ,
	    ICP_FLAG_HIT_OBJ,
	    entry->url,
	    icpHitObjState->header.reqnum,
	    icpHitObjState->pad,
	    entry);
	if (reply)
	    icpUdpSend(icpHitObjState->fd,
		&icpHitObjState->to,
		reply,
		LOG_UDP_HIT_OBJ,
		urlParseProtocol(entry->url));
    }
    storeUnlockObject(entry);
    safe_free(icpHitObjState);
}

static int
icpCheckUdpHit(StoreEntry * e, request_t * request)
{
    if (e == NULL)
	return 0;
    if (!storeEntryValidToSend(e))
	return 0;
    if (refreshCheck(e, request, 30))
	return 0;
    return 1;
}

static int
icpCheckUdpHitObj(StoreEntry * e, request_t * r, icp_common_t * h, int len)
{
    if (!BIT_TEST(h->flags, ICP_FLAG_HIT_OBJ))	/* not requested */
	return 0;
    if (len > SQUID_UDP_SO_SNDBUF)	/* too big */
	return 0;
    if (refreshCheck(e, r, 0))	/* stale */
	return 0;
#ifdef MEM_UDP_HIT_OBJ
    if (e->mem_status != IN_MEMORY)
	return 0;
#endif
    return 1;
}

static void
icpHandleIcpV2(int fd, struct sockaddr_in from, char *buf, int len)
{
    icp_common_t header;
    icp_common_t *headerp = (icp_common_t *) (void *) buf;
    StoreEntry *entry = NULL;
    char *url = NULL;
    const char *key = NULL;
    request_t *icp_request = NULL;
    int allow = 0;
    char *data = NULL;
    u_short data_sz = 0;
    u_short u;
    icpHitObjStateData *icpHitObjState = NULL;
    int pkt_len;
    aclCheck_t checklist;
    icp_common_t *reply;
    int netdb_gunk = 0;

    header.opcode = headerp->opcode;
    header.version = headerp->version;
    header.length = ntohs(headerp->length);
    header.reqnum = ntohl(headerp->reqnum);
    header.flags = ntohl(headerp->flags);
    /* xmemcpy(headerp->auth, , ICP_AUTH_SIZE); */
    header.shostid = ntohl(headerp->shostid);

    switch (header.opcode) {
    case ICP_OP_QUERY:
	nudpconn++;
	/* We have a valid packet */
	url = buf + sizeof(header) + sizeof(u_num32);
	if ((icp_request = urlParse(METHOD_GET, url)) == NULL) {
	    reply = icpCreateMessage(ICP_OP_ERR, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_INVALID, PROTO_NONE);
	    break;
	}
	checklist.src_addr = from.sin_addr;
	checklist.request = icp_request;
	allow = aclCheck(ICPAccessList, &checklist);
	if (!allow) {
	    debug(12, 2, "icpHandleIcpV2: Access Denied for %s by %s.\n",
		inet_ntoa(from.sin_addr), AclMatchedName);
	    if (clientdbDeniedPercent(from.sin_addr) < 95) {
		reply = icpCreateMessage(ICP_OP_DENIED, 0, url, header.reqnum, 0);
		icpUdpSend(fd, &from, reply, LOG_UDP_DENIED, icp_request->protocol);
	    }
	    break;
	}
	if (header.flags & ICP_FLAG_NETDB_GUNK) {
	    int rtt, hops;
	    rtt = netdbHostRtt(icp_request->host);
	    hops = netdbHostHops(icp_request->host);
	    netdb_gunk = htonl(((hops & 0xFFFF) << 16) | (rtt & 0xFFFF));
	}
	/* The peer is allowed to use this cache */
	entry = storeGet(storeGeneratePublicKey(url, METHOD_GET));
	debug(12, 5, "icpHandleIcpV2: OPCODE %s\n", IcpOpcodeStr[header.opcode]);
	if (icpCheckUdpHit(entry, icp_request)) {
	    pkt_len = sizeof(icp_common_t) + strlen(url) + 1 + 2 + entry->object_len;
	    reply = icpCreateMessage(ICP_OP_HIT, 0, url, header.reqnum, netdb_gunk);
	    icpUdpSend(fd, &from, reply, LOG_UDP_HIT, icp_request->protocol);
	    break;
	}
	/* if store is rebuilding, return a UDP_HIT, but not a MISS */
	if (store_rebuilding == STORE_REBUILDING_FAST && opt_reload_hit_only) {
	    reply = icpCreateMessage(ICP_OP_RELOADING, 0, url, header.reqnum, netdb_gunk);
	    icpUdpSend(fd, &from, reply, LOG_UDP_RELOADING, icp_request->protocol);
	} else if (hit_only_mode_until > squid_curtime) {
	    reply = icpCreateMessage(ICP_OP_RELOADING, 0, url, header.reqnum, netdb_gunk);
	    icpUdpSend(fd, &from, reply, LOG_UDP_RELOADING, icp_request->protocol);
	} else {
	    reply = icpCreateMessage(ICP_OP_MISS, 0, url, header.reqnum, netdb_gunk);
	    icpUdpSend(fd, &from, reply, LOG_UDP_MISS, icp_request->protocol);
	}
	break;

    case ICP_OP_HIT_OBJ:
    case ICP_OP_HIT:
    case ICP_OP_SECHO:
    case ICP_OP_DECHO:
    case ICP_OP_MISS:
    case ICP_OP_DENIED:
    case ICP_OP_RELOADING:
	if (neighbors_do_private_keys && header.reqnum == 0) {
	    debug(12, 0, "icpHandleIcpV2: Neighbor %s returned reqnum = 0\n",
		inet_ntoa(from.sin_addr));
	    debug(12, 0, "icpHandleIcpV2: Disabling use of private keys\n");
	    neighbors_do_private_keys = 0;
	}
	url = buf + sizeof(header);
	if (header.opcode == ICP_OP_HIT_OBJ) {
	    data = url + strlen(url) + 1;
	    xmemcpy((char *) &u, data, sizeof(u_short));
	    data += sizeof(u_short);
	    data_sz = ntohs(u);
	    if ((int) data_sz > (len - (data - buf))) {
		debug(12, 0, "icpHandleIcpV2: ICP_OP_HIT_OBJ object too small\n");
		break;
	    }
	}
	debug(12, 3, "icpHandleIcpV2: %s from %s for '%s'\n",
	    IcpOpcodeStr[header.opcode],
	    inet_ntoa(from.sin_addr),
	    url);
	if (neighbors_do_private_keys && header.reqnum) {
	    key = storeGeneratePrivateKey(url, METHOD_GET, header.reqnum);
	} else {
	    key = storeGeneratePublicKey(url, METHOD_GET);
	}
	debug(12, 3, "icpHandleIcpV2: Looking for key '%s'\n", key);
	if ((entry = storeGet(key)) == NULL) {
	    debug(12, 3, "icpHandleIcpV2: Ignoring %s for NULL Entry.\n",
		IcpOpcodeStr[header.opcode]);
	} else {
	    /* call neighborsUdpAck even if ping_status != PING_WAITING */
	    neighborsUdpAck(fd,
		url,
		&header,
		&from,
		entry,
		data,
		(int) data_sz);
	}
	break;

    case ICP_OP_INVALID:
	break;

    default:
	debug(12, 0, "icpHandleIcpV2: UNKNOWN OPCODE: %d from %s\n",
	    header.opcode, inet_ntoa(from.sin_addr));
	break;
    }
    if (icp_request)
	put_free_request_t(icp_request);
}

/* Currently Harvest cached-2.x uses ICP_VERSION_3 */
static void
icpHandleIcpV3(int fd, struct sockaddr_in from, char *buf, int len)
{
    icp_common_t header;
    icp_common_t *reply;
    icp_common_t *headerp = (icp_common_t *) (void *) buf;
    StoreEntry *entry = NULL;
    char *url = NULL;
    const char *key = NULL;
    request_t *icp_request = NULL;
    int allow = 0;
    char *data = NULL;
    u_short data_sz = 0;
    u_short u;
    aclCheck_t checklist;

    header.opcode = headerp->opcode;
    header.version = headerp->version;
    header.length = ntohs(headerp->length);
    header.reqnum = ntohl(headerp->reqnum);
    header.flags = ntohl(headerp->flags);
    /* xmemcpy(headerp->auth, , ICP_AUTH_SIZE); */
    header.shostid = ntohl(headerp->shostid);

    switch (header.opcode) {
    case ICP_OP_QUERY:
	nudpconn++;
	/* We have a valid packet */
	url = buf + sizeof(header) + sizeof(u_num32);
	if ((icp_request = urlParse(METHOD_GET, url)) == NULL) {
	    reply = icpCreateMessage(ICP_OP_ERR, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_INVALID, PROTO_NONE);
	    break;
	}
	checklist.src_addr = from.sin_addr;
	checklist.request = icp_request;
	allow = aclCheck(ICPAccessList, &checklist);
	if (!allow) {
	    debug(12, 2, "icpHandleIcpV3: Access Denied for %s by %s.\n",
		inet_ntoa(from.sin_addr), AclMatchedName);
	    if (clientdbDeniedPercent(from.sin_addr) < 95) {
		reply = icpCreateMessage(ICP_OP_DENIED, 0, url, header.reqnum, 0);
		icpUdpSend(fd, &from, reply, LOG_UDP_DENIED, icp_request->protocol);
	    }
	    break;
	}
	/* The peer is allowed to use this cache */
	entry = storeGet(storeGeneratePublicKey(url, METHOD_GET));
	debug(12, 5, "icpHandleIcpV3: OPCODE %s\n",
	    IcpOpcodeStr[header.opcode]);
	if (icpCheckUdpHit(entry, icp_request)) {
	    reply = icpCreateMessage(ICP_OP_HIT, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_HIT, icp_request->protocol);
	    break;
	}
	/* if store is rebuilding, return a UDP_HIT, but not a MISS */
	if (opt_reload_hit_only && store_rebuilding == STORE_REBUILDING_FAST) {
	    reply = icpCreateMessage(ICP_OP_RELOADING, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_RELOADING, icp_request->protocol);
	} else if (hit_only_mode_until > squid_curtime) {
	    reply = icpCreateMessage(ICP_OP_RELOADING, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_RELOADING, icp_request->protocol);
	} else {
	    reply = icpCreateMessage(ICP_OP_MISS, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_MISS, icp_request->protocol);
	}
	break;

    case ICP_OP_HIT_OBJ:
    case ICP_OP_HIT:
    case ICP_OP_SECHO:
    case ICP_OP_DECHO:
    case ICP_OP_MISS:
    case ICP_OP_DENIED:
	if (neighbors_do_private_keys && header.reqnum == 0) {
	    debug(12, 0, "icpHandleIcpV3: Neighbor %s returned reqnum = 0\n",
		inet_ntoa(from.sin_addr));
	    debug(12, 0, "icpHandleIcpV3: Disabling use of private keys\n");
	    neighbors_do_private_keys = 0;
	}
	url = buf + sizeof(header);
	if (header.opcode == ICP_OP_HIT_OBJ) {
	    data = url + strlen(url) + 1;
	    xmemcpy((char *) &u, data, sizeof(u_short));
	    data += sizeof(u_short);
	    data_sz = ntohs(u);
	    if ((int) data_sz > (len - (data - buf))) {
		debug(12, 0, "icpHandleIcpV3: ICP_OP_HIT_OBJ object too small\n");
		break;
	    }
	}
	debug(12, 3, "icpHandleIcpV3: %s from %s for '%s'\n",
	    IcpOpcodeStr[header.opcode],
	    inet_ntoa(from.sin_addr),
	    url);
	if (neighbors_do_private_keys && header.reqnum) {
	    key = storeGeneratePrivateKey(url, METHOD_GET, header.reqnum);
	} else {
	    key = storeGeneratePublicKey(url, METHOD_GET);
	}
	debug(12, 3, "icpHandleIcpV3: Looking for key '%s'\n", key);
	if ((entry = storeGet(key)) == NULL) {
	    debug(12, 3, "icpHandleIcpV3: Ignoring %s for NULL Entry.\n",
		IcpOpcodeStr[header.opcode]);
	} else {
	    /* call neighborsUdpAck even if ping_status != PING_WAITING */
	    neighborsUdpAck(fd,
		url,
		&header,
		&from,
		entry,
		data,
		(int) data_sz);
	}
	break;

    case ICP_OP_INVALID:
    case ICP_OP_ERR:
	break;

    default:
	debug(12, 0, "icpHandleIcpV3: UNKNOWN OPCODE: %d from %s\n",
	    header.opcode, inet_ntoa(from.sin_addr));
	break;
    }
    if (icp_request)
	put_free_request_t(icp_request);
}

#ifdef ICP_PKT_DUMP
static void
icpPktDump(icp_common_t * pkt)
{
    struct in_addr a;
    debug(12, 9, "opcode:     %3d %s\n",
	(int) pkt->opcode,
	IcpOpcodeStr[pkt->opcode]);
    debug(12, 9, "version: %-8d\n", (int) pkt->version);
    debug(12, 9, "length:  %-8d\n", (int) ntohs(pkt->length));
    debug(12, 9, "reqnum:  %-8d\n", ntohl(pkt->reqnum));
    debug(12, 9, "flags:   %-8x\n", ntohl(pkt->flags));
    a.s_addr = ntohl(pkt->shostid);
    debug(12, 9, "shostid: %s\n", inet_ntoa(a));
    debug(12, 9, "payload: %s\n", (char *) pkt + sizeof(icp_common_t));
}
#endif

void
icpHandleUdp(int sock, void *not_used)
{
    struct sockaddr_in from;
    int from_len;
    LOCAL_ARRAY(char, buf, SQUID_UDP_SO_RCVBUF);
    int len;
    icp_common_t *headerp = NULL;
    int icp_version;

    commSetSelect(sock, COMM_SELECT_READ, icpHandleUdp, NULL, 0);
    from_len = sizeof(from);
    memset(&from, 0, from_len);
    len = recvfrom(sock,
	buf,
	SQUID_UDP_SO_RCVBUF - 1,
	0,
	(struct sockaddr *) &from,
	&from_len);
    if (len < 0) {
#ifdef _SQUID_LINUX_
	/* Some Linux systems seem to set the FD for reading and then
	 * return ECONNREFUSED when sendto() fails and generates an ICMP
	 * port unreachable message. */
	if (errno != ECONNREFUSED)
#endif
	    debug(50, 1, "icpHandleUdp: FD %d recvfrom: %s\n",
		sock, xstrerror());
	return;
    }
    buf[len] = '\0';
    debug(12, 4, "icpHandleUdp: FD %d: received %d bytes from %s.\n",
	sock,
	len,
	inet_ntoa(from.sin_addr));
#ifdef ICP_PACKET_DUMP
    icpPktDump(buf);
#endif
    if (len < sizeof(icp_common_t)) {
	debug(12, 4, "icpHandleUdp: Ignoring too-small UDP packet\n");
	return;
    }
    headerp = (icp_common_t *) (void *) buf;
    if ((icp_version = (int) headerp->version) == ICP_VERSION_2)
	icpHandleIcpV2(sock, from, buf, len);
    else if (icp_version == ICP_VERSION_3)
	icpHandleIcpV3(sock, from, buf, len);
    else
	debug(12, 0, "Unused ICP version %d received from %s:%d\n",
	    icp_version,
	    inet_ntoa(from.sin_addr),
	    ntohs(from.sin_port));
}

static char *
do_append_domain(const char *url, const char *ad)
{
    char *b = NULL;		/* beginning of hostname */
    char *e = NULL;		/* end of hostname */
    char *p = NULL;
    char *u = NULL;
    int lo;
    int ln;
    int adlen;

    if (!(b = strstr(url, "://")))	/* find beginning of host part */
	return NULL;
    b += 3;
    if (!(e = strchr(b, '/')))	/* find end of host part */
	e = b + strlen(b);
    if ((p = strchr(b, '@')) && p < e)	/* After username info */
	b = p + 1;
    if ((p = strchr(b, ':')) && p < e)	/* Before port */
	e = p;
    if ((p = strchr(b, '.')) && p < e)	/* abort if host has dot already */
	return NULL;
    lo = strlen(url);
    ln = lo + (adlen = strlen(ad));
    u = xcalloc(ln + 1, 1);
    strncpy(u, url, (e - url));	/* copy first part */
    b = u + (e - url);
    p = b + adlen;
    strncpy(b, ad, adlen);	/* copy middle part */
    strncpy(p, e, lo - (e - url));	/* copy last part */
    return (u);
}


/*
 *  parseHttpRequest()
 * 
 *  Called by
 *    asciiProcessInput() after the request has been read
 *  Calls
 *    do_append_domain()
 *  Returns
 *   -1 on error
 *    0 on incomplete request
 *    1 on success
 */
static int
parseHttpRequest(icpStateData * icpState)
{
    char *inbuf = NULL;
    char *method = NULL;
    char *url = NULL;
    char *req_hdr = NULL;
    LOCAL_ARRAY(char, http_ver, 32);
    char *token = NULL;
    char *t = NULL;
    char *s = NULL;
    char *ad = NULL;
    int free_request = 0;
    int req_hdr_sz;
    int len;

    /* Make sure a complete line has been received */
    if (strchr(icpState->in.buf, '\n') == NULL) {
	debug(12, 5, "Incomplete request line, waiting for more data\n");
	return 0;
    }
    /* Use xmalloc/xmemcpy instead of xstrdup because inbuf might
     * contain NULL bytes; especially for POST data  */
    inbuf = xmalloc(icpState->in.offset + 1);
    xstrncpy(inbuf, icpState->in.buf, icpState->in.offset + 1);

    /* Look for request method */
    if ((method = strtok(inbuf, "\t ")) == NULL) {
	debug(12, 1, "parseHttpRequest: Can't get request method\n");
	xfree(inbuf);
	return -1;
    }
    icpState->method = urlParseMethod(method);
    if (icpState->method == METHOD_NONE) {
	debug(12, 1, "parseHttpRequest: Unsupported method '%s'\n", method);
	xfree(inbuf);
	return -1;
    }
    debug(12, 5, "parseHttpRequest: Method is '%s'\n", method);

    /* look for URL */
    if ((url = strtok(NULL, "\r\n\t ")) == NULL) {
	debug(12, 1, "parseHttpRequest: Missing URL\n");
	xfree(inbuf);
	return -1;
    }
    debug(12, 5, "parseHttpRequest: Request is '%s'\n", url);

    token = strtok(NULL, null_string);
    for (t = token; t && *t && *t != '\n' && *t != '\r'; t++);
    if (t == NULL || *t == '\0' || t == token) {
	debug(12, 3, "parseHttpRequest: Missing HTTP identifier\n");
	xfree(inbuf);
	return -1;
    }
    len = (int) (t - token);
    memset(http_ver, '\0', 32);
    strncpy(http_ver, token, len < 31 ? len : 31);
    sscanf(http_ver, "HTTP/%f", &icpState->http_ver);
    debug(12, 5, "parseHttpRequest: HTTP version is '%s'\n", http_ver);

    /* Check if headers are received */
    if (!mime_headers_end(t)) {
	xfree(inbuf);
	return 0;		/* not a complete request */
    }
    while (isspace(*t))
	t++;
    req_hdr = t;
    req_hdr_sz = icpState->in.offset - (req_hdr - inbuf);

    /* Ok, all headers are received */
    icpState->req_hdr_sz = req_hdr_sz;
    icpState->request_hdr = xmalloc(req_hdr_sz + 1);
    xmemcpy(icpState->request_hdr, req_hdr, req_hdr_sz);
    *(icpState->request_hdr + req_hdr_sz) = '\0';

    debug(12, 5, "parseHttpRequest: Request Header is\n%s\n",
	icpState->request_hdr);

    /* Assign icpState->url */
    if ((t = strchr(url, '\n')))	/* remove NL */
	*t = '\0';
    if ((t = strchr(url, '\r')))	/* remove CR */
	*t = '\0';
    if ((t = strchr(url, '#')))	/* remove HTML anchors */
	*t = '\0';

    if ((ad = Config.appendDomain)) {
	if ((t = do_append_domain(url, ad))) {
	    if (free_request)
		safe_free(url);
	    url = t;
	    free_request = 1;
	    /* NOTE: We don't have to free the old request pointer
	     * because it points to inside inbuf. But
	     * do_append_domain() allocates new memory so set a flag
	     * if the request should be freed later. */
	}
    }
    /* see if we running in httpd_accel_mode, if so got to convert it to URL */
    if (httpd_accel_mode && *url == '/') {
	/* prepend the accel prefix */
	if (vhost_mode) {
	    /* Put the local socket IP address as the hostname */
	    icpState->url = xcalloc(strlen(url) + 32, 1);
	    sprintf(icpState->url, "http://%s:%d%s",
		inet_ntoa(icpState->me.sin_addr),
		(int) Config.Accel.port,
		url);
	    debug(12, 5, "VHOST REWRITE: '%s'\n", icpState->url);
	} else if (opt_accel_uses_host && (t = mime_get_header(req_hdr, "Host"))) {
	    /* If a Host: header was specified, use it to build the URL 
	     * instead of the one in the Config file. */
	    /*
	     * XXX Use of the Host: header here opens a potential
	     * security hole.  There are no checks that the Host: value
	     * corresponds to one of your servers.  It might, for example,
	     * refer to www.playboy.com.  The 'dst' and/or 'dst_domain' ACL 
	     * types should be used to prevent httpd-accelerators 
	     * handling requests for non-local servers */
	    if ((s = strchr(t, ':')))
		*s = '\0';
	    icpState->url = xcalloc(strlen(url) + strlen(t) + 32, 1);
	    sprintf(icpState->url, "http://%s:%d%s",
		t, (int) Config.Accel.port, url);
	} else {
	    icpState->url = xcalloc(strlen(Config.Accel.prefix) +
		strlen(url) + 1, 1);
	    sprintf(icpState->url, "%s%s", Config.Accel.prefix, url);
	}
	icpState->accel = 1;
    } else {
	/* URL may be rewritten later, so make extra room */
	icpState->url = xcalloc(strlen(url) + 5, 1);
	strcpy(icpState->url, url);
	icpState->accel = 0;
    }

    debug(12, 5, "parseHttpRequest: Complete request received\n");
    if (free_request)
	safe_free(url);
    xfree(inbuf);
    return 1;
}

#define ASCII_INBUF_BLOCKSIZE 4096

static void
asciiProcessInput(int fd, char *buf, int size, int flag, void *data)
{
    icpStateData *icpState = data;
    int parser_return_code = 0;
    int k;
    request_t *request = NULL;
    char *wbuf = NULL;

    debug(12, 4, "asciiProcessInput: FD %d: reading request...\n", fd);
    debug(12, 4, "asciiProcessInput: size = %d\n", size);

    if (flag != COMM_OK) {
	/* connection closed by foreign host */
	comm_close(fd);
	return;
    }
    icpState->in.offset += size;
    icpState->in.buf[icpState->in.offset] = '\0';	/* Terminate the string */

    parser_return_code = parseHttpRequest(icpState);
    if (parser_return_code == 1) {
	if ((request = urlParse(icpState->method, icpState->url)) == NULL) {
	    debug(12, 5, "Invalid URL: %s\n", icpState->url);
	    wbuf = squid_error_url(icpState->url,
		icpState->method,
		ERR_INVALID_URL,
		fd_table[fd].ipaddr,
		icpState->http_code,
		NULL);
	    icpSendERROR(fd, ERR_INVALID_URL, wbuf, icpState, 400);
	    return;
	}
	request->http_ver = icpState->http_ver;
	if (!urlCheckRequest(request)) {
	    icpState->log_type = ERR_UNSUP_REQ;
	    icpState->http_code = 501;
	    wbuf = xstrdup(squid_error_url(icpState->url,
		    icpState->method,
		    ERR_UNSUP_REQ,
		    fd_table[fd].ipaddr,
		    icpState->http_code,
		    NULL));
	    comm_write(fd,
		wbuf,
		strlen(wbuf),
		30,
		icpSendERRORComplete,
		(void *) icpState,
		xfree);
	    return;
	}
	icpState->request = requestLink(request);
	clientAccessCheck(icpState, clientAccessCheckDone);
    } else if (parser_return_code == 0) {
	/*
	 *    Partial request received; reschedule until parseAsciiUrl()
	 *    is happy with the input
	 */
	k = icpState->in.size - 1 - icpState->in.offset;
	if (k == 0) {
	    if (icpState->in.offset >= Config.maxRequestSize) {
		/* The request is too large to handle */
		debug(12, 0, "asciiProcessInput: Request won't fit in buffer.\n");
		debug(12, 0, "-->     max size = %d\n", Config.maxRequestSize);
		debug(12, 0, "--> icpState->in.offset = %d\n", icpState->in.offset);
		icpSendERROR(fd,
		    ERR_INVALID_REQ,
		    "error reading request",
		    icpState,
		    400);
		return;
	    }
	    /* Grow the request memory area to accomodate for a large request */
	    icpState->in.size += ASCII_INBUF_BLOCKSIZE;
	    icpState->in.buf = xrealloc(icpState->in.buf, icpState->in.size);
	    meta_data.misc += ASCII_INBUF_BLOCKSIZE;
	    debug(12, 2, "Handling a large request, offset=%d inbufsize=%d\n",
		icpState->in.offset, icpState->in.size);
	    k = icpState->in.size - 1 - icpState->in.offset;
	}
	comm_read(fd,
	    icpState->in.buf + icpState->in.offset,
	    k,			/* size */
	    30,			/* timeout */
	    TRUE,		/* handle immed */
	    asciiProcessInput,
	    (void *) icpState);
    } else {
	/* parser returned -1 */
	debug(12, 1, "asciiProcessInput: FD %d Invalid Request\n", fd);
	wbuf = squid_error_request(icpState->in.buf,
	    ERR_INVALID_REQ,
	    fd_table[fd].ipaddr,
	    icpState->http_code);
	icpSendERROR(fd, ERR_INVALID_REQ, wbuf, icpState, 400);
    }
}


/* general lifetime handler for ascii connection */
static void
asciiConnLifetimeHandle(int fd, icpStateData * icpState)
{
    int x;
    StoreEntry *entry = icpState->entry;
    debug(12, 2, "asciiConnLifetimeHandle: FD %d: lifetime is expired.\n", fd);
    CheckQuickAbort(icpState);
    if (entry)
	storeUnregister(entry, fd);
    x = protoUnregister(fd,
	entry,
	icpState->request,
	icpState->peer.sin_addr);
    if (x != 0)
	return;
    if (entry == NULL) {
	comm_close(fd);
	return;
    }
    if (entry->store_status == STORE_PENDING)
	storeAbort(entry);
}

/* Handle a new connection on ascii input socket. */
void
asciiHandleConn(int sock, void *notused)
{
    int fd = -1;
    int lft = -1;
    icpStateData *icpState = NULL;
    struct sockaddr_in peer;
    struct sockaddr_in me;

    memset((char *) &peer, '\0', sizeof(struct sockaddr_in));
    memset((char *) &me, '\0', sizeof(struct sockaddr_in));
    commSetSelect(sock, COMM_SELECT_READ, asciiHandleConn, NULL, 0);
    if ((fd = comm_accept(sock, &peer, &me)) < 0) {
	debug(50, 1, "asciiHandleConn: FD %d: accept failure: %s\n",
	    sock, xstrerror());
	return;
    }
    if (vizSock > -1)
	vizHackSendPkt(&peer, 1);
    /* set the hardwired lifetime */
    lft = comm_set_fd_lifetime(fd, Config.lifetimeDefault);
    ntcpconn++;

    debug(12, 4, "asciiHandleConn: FD %d: accepted, lifetime %d\n", fd, lft);

    icpState = xcalloc(1, sizeof(icpStateData));
    icpState->start = current_time;
    icpState->in.size = ASCII_INBUF_BLOCKSIZE;
    icpState->in.buf = xcalloc(icpState->in.size, 1);
    icpState->out.buf = get_free_8k_page();
    icpState->header.shostid = htonl(peer.sin_addr.s_addr);
    icpState->peer = peer;
    icpState->log_addr = peer.sin_addr;
    icpState->log_addr.s_addr &= Config.Addrs.client_netmask.s_addr;
    icpState->me = me;
    icpState->entry = NULL;
    icpState->fd = fd;
    icpState->swapin_fd = -1;
    icpState->ident.fd = -1;
    fd_note(fd, inet_ntoa(icpState->log_addr));
    meta_data.misc += ASCII_INBUF_BLOCKSIZE;
    commSetSelect(fd,
	COMM_SELECT_LIFETIME,
	(PF) asciiConnLifetimeHandle,
	(void *) icpState, 0);
    comm_add_close_handler(fd,
	icpStateFree,
	(void *) icpState);
    comm_read(fd,
	icpState->in.buf,
	icpState->in.size - 1,	/* size */
	30,			/* timeout */
	1,			/* handle immed */
	asciiProcessInput,
	(void *) icpState);
    /* start reverse lookup */
    if (Config.Log.log_fqdn)
	fqdncache_gethostbyaddr(peer.sin_addr, FQDN_LOOKUP_IF_MISS);
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
CheckQuickAbort2(const icpStateData * icpState)
{
    long curlen;
    long minlen;
    long expectlen;
    if (!BIT_TEST(icpState->request->flags, REQ_CACHABLE))
	return 1;
    if (BIT_TEST(icpState->entry->flag, KEY_PRIVATE))
	return 1;
    if (icpState->entry->mem_obj == NULL)
	return 1;
    expectlen = icpState->entry->mem_obj->reply->content_length;
    curlen = icpState->entry->object_len;
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
CheckQuickAbort(icpStateData * icpState)
{
    if (icpState->entry == NULL)
	return;
    if (storePendingNClients(icpState->entry) > 1)
	return;
    if (icpState->entry->store_status == STORE_OK)
	return;
    if (CheckQuickAbort2(icpState) == 0)
	return;
    BIT_SET(icpState->entry->flag, CLIENT_ABORT_REQUEST);
    storeReleaseRequest(icpState->entry);
    icpState->log_type = ERR_CLIENT_ABORT;
}

static int
icpCheckTransferDone(icpStateData * icpState)
{
    StoreEntry *entry = icpState->entry;
    MemObject *mem = NULL;
    if (entry == NULL)
	return 0;
    debug(0, 0, "icpCheckTransferDone: %s\n", entry->url);
    debug(0, 0, "  store_status %s\n", storeStatusStr[entry->store_status]);
    debug(0, 0, "   swap_status %s\n", swapStatusStr[entry->swap_status]);
    debug(0, 0, "    object_len %d\n", entry->object_len);
    debug(0, 0, "      out.size %d\n", icpState->out.size);
    if (entry->store_status != STORE_PENDING)
	if (icpState->out.size >= entry->object_len)
	    return 1;
    if ((mem = entry->mem_obj) == NULL)
	return 0;
    if (mem->reply->content_length == 0)
	return 0;
    if (icpState->out.size >= mem->reply->content_length + mem->reply->hdr_sz)
	return 1;
    return 0;
}

void
icpDetectClientClose(int fd, void *data)
{
    icpStateData *icpState = data;
    LOCAL_ARRAY(char, buf, 256);
    int n;
    StoreEntry *entry = icpState->entry;
    errno = 0;

    if (icpCheckTransferDone(icpState)) {
	/* All data has been delivered */
	debug(12, 5, "icpDetectClientClose: FD %d end of transmission\n", fd);
	HTTPCacheInfo->proto_touchobject(HTTPCacheInfo,
	    HTTPCacheInfo->proto_id(entry->url),
	    icpState->out.size);
	comm_close(fd);
    } else if ((n = read(fd, buf, 255)) > 0) {
	buf[n] = '\0';
	debug(12, 0, "icpDetectClientClose: FD %d, %d unexpected bytes\n",
	    fd, n);
	debug(12, 1, "--> from: %s\n", fd_table[fd].ipaddr);
	debug(12, 1, "--> data: %s\n", rfc1738_escape(buf));
	commSetSelect(fd,
	    COMM_SELECT_READ,
	    icpDetectClientClose,
	    (void *) icpState,
	    0);
    } else if (n < 0) {
	debug(12, 5, "icpDetectClientClose: FD %d\n", fd);
	debug(12, 5, "--> URL '%s'\n", icpState->url);
	if (errno == ECONNRESET)
	    debug(50, 2, "icpDetectClientClose: ERROR %s: %s\n",
		fd_table[fd].ipaddr, xstrerror());
	else if (errno)
	    debug(50, 1, "icpDetectClientClose: ERROR %s: %s\n",
		fd_table[fd].ipaddr, xstrerror());
	commCancelRWHandler(fd);
	CheckQuickAbort(icpState);
	if (entry) {
	    if (entry->ping_status == PING_WAITING)
		storeReleaseRequest(entry);
	    storeUnregister(entry, fd);
	    storeRegister(entry, fd, icpHandleAbort, (void *) icpState,
		icpState->out.offset);
	} else
	    comm_close(fd);
    } else {
	debug(12, 5, "icpDetectClientClose: FD %d closed?\n", fd);
	comm_set_stall(fd, 10);	/* check again in 10 seconds */
    }
}

void
icpDetectNewRequest(int fd)
{
#if TRY_KEEPALIVE_SUPPORT
    int lft = -1;
    icpStateData *icpState = NULL;
    struct sockaddr_in me;
    struct sockaddr_in peer;
    int len;
    /* set the hardwired lifetime */
    lft = comm_set_fd_lifetime(fd, Config.lifetimeDefault);
    len = sizeof(struct sockaddr_in);
    memset((char *) &me, '\0', len);
    memset((char *) &peer, '\0', len);
    if (getsockname(fd, (struct sockaddr *) &me, &len) < -1) {
	debug(50, 1, "icpDetectNewRequest: FD %d: getsockname failure: %s\n",
	    fd, xstrerror());
	return;
    }
    len = sizeof(struct sockaddr_in);
    if (getpeername(fd, (struct sockaddr *) &peer, &len) < -1) {
	debug(50, 1, "icpDetectNewRequest: FD %d: getpeername failure: %s\n",
	    fd, xstrerror());
	return;
    }
    ntcpconn++;
    if (vizSock > -1)
	vizHackSendPkt(&peer, 1);
    debug(12, 4, "icpDetectRequest: FD %d: accepted, lifetime %d\n", fd, lft);
    icpState = xcalloc(1, sizeof(icpStateData));
    icpState->start = current_time;
    icpState->in.size = ASCII_INBUF_BLOCKSIZE;
    icpState->in.buf = xcalloc(icpState->in.size, 1);
    icpState->header.shostid = htonl(peer.sin_addr.s_addr);
    icpState->peer = peer;
    icpState->log_addr = peer.sin_addr;
    icpState->log_addr.s_addr &= Config.Addrs.client_netmask.s_addr;
    icpState->me = me;
    icpState->entry = NULL;
    icpState->fd = fd;
    fd_note(fd, inet_ntoa(icpState->log_addr));
    meta_data.misc += ASCII_INBUF_BLOCKSIZE;
    comm_add_close_handler(fd,
	icpStateFree,
	(void *) icpState);
    comm_read(fd,
	icpState->in.buf,
	icpState->in.size - 1,	/* size */
	30,			/* timeout */
	1,			/* handle immed */
	asciiProcessInput,
	(void *) icpState);
    /* start reverse lookup */
    if (Config.Log.log_fqdn)
	fqdncache_gethostbyaddr(peer.sin_addr, FQDN_LOOKUP_IF_MISS);
#endif
}

static char *
icpConstruct304reply(struct _http_reply *source)
{
    LOCAL_ARRAY(char, line, 256);
    LOCAL_ARRAY(char, reply, 8192);
    memset(reply, '\0', 8192);
    strcpy(reply, "HTTP/1.0 304 Not Modified\r\n");
    if (source->date > -1) {
	sprintf(line, "Date: %s\r\n", mkrfc1123(source->date));
	strcat(reply, line);
    }
    if ((int) strlen(source->content_type) > 0) {
	sprintf(line, "Content-type: %s\r\n", source->content_type);
	strcat(reply, line);
    }
    if (source->content_length) {
	sprintf(line, "Content-length: %d\r\n", source->content_length);
	strcat(reply, line);
    }
    if (source->expires > -1) {
	sprintf(line, "Expires: %s\r\n", mkrfc1123(source->expires));
	strcat(reply, line);
    }
    if (source->last_modified > -1) {
	sprintf(line, "Last-modified: %s\r\n",
	    mkrfc1123(source->last_modified));
	strcat(reply, line);
    }
    strcat(reply, "\r\n");
    return reply;
}

struct viz_pkt {
    u_num32 from;
    char type;
};

void
vizHackSendPkt(const struct sockaddr_in *from, int type)
{
    static struct viz_pkt v;
    v.from = from->sin_addr.s_addr;
    v.type = (char) type;
    sendto(vizSock,
	(char *) &v,
	sizeof(v),
	0,
	(struct sockaddr *) &Config.vizHack.S,
	sizeof(struct sockaddr_in));
}

/* 
 * icpHandleAbort()
 * Call for objects which might have been aborted.  If the entry
 * was aborted, AND the client has not seen any data yet, then
 * Queue the error page via icpSendERROR().  Otherwise just
 * close the socket.
 */
void
icpHandleAbort(int fd, void *data)
{
    icpStateData *icpState = data;
    StoreEntry *entry = icpState->entry;
    if (entry == NULL) {
	comm_close(fd);
	return;
    }
    if (entry->store_status != STORE_ABORTED) {
	comm_close(fd);
	return;
    }
    if (icpState->out.size > 0) {
	comm_close(fd);
	return;
    }
    if (entry->mem_obj->e_abort_msg == NULL) {
	comm_close(fd);
	return;
    }
    icpSendERROR(fd,
	entry->mem_obj->abort_code,
	entry->mem_obj->e_abort_msg,
	icpState,
	400);
}


/*
 * $Id$
 *
 * DEBUG: section 33    Client-side Routines
 * AUTHOR: Duane Wessels
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

/* Errors and client side 
 *
 * Problem the first: the store entry is no longer authoritative on the
 * reply status. EBITTEST (E_ABORT) is no longer a valid test outside
 * of client_side_reply.c.
 * Problem the second: resources are wasted if we delay in cleaning up.
 * Problem the third we can't depend on a connection close to clean up.
 * 
 * Nice thing the first: Any step in the stream can callback with data 
 * representing an error.
 * Nice thing the second: once you stop requesting reads from upstream,
 * upstream can be stopped too.
 *
 * Solution #1: Error has a callback mechanism to hand over a membuf
 * with the error content. The failing node pushes that back as the 
 * reply. Can this be generalised to reduce duplicate efforts?
 * A: Possibly. For now, only one location uses this.
 * How to deal with pre-stream errors?
 * Tell client_side_reply that we *want* an error page before any
 * stream calls occur. Then we simply read as normal.
 */

#include "squid.h"
#include "clientStream.h"
#include "IPInterception.h"

#if LINGERING_CLOSE
#define comm_close comm_lingering_close
#endif

#define FAILURE_MODE_TIME 300

/* Persistent connection logic:
 *
 * requests (httpClientRequest structs) get added to the connection
 * list, with the current one being chr
 * 
 * The request is *immediately* kicked off, and data flows through
 * to clientSocketRecipient.
 * 
 * If the data that arrives at clientSocketRecipient is not for the current
 * request, clientSocketRecipient simply returns, without requesting more
 * data, or sending it.
 *
 * ClientKeepAliveNextRequest will then detect the presence of data in 
 * the next clientHttpRequest, and will send it, restablishing the 
 * data flow.
 */

/* our socket-related context */
typedef struct _clientSocketContext {
    clientHttpRequest *http;	/* we own this */
    char reqbuf[HTTP_REQBUF_SZ];
    struct _clientSocketContext *next;
    struct {
	int deferred:1;		/* This is a pipelined request waiting for the
				 * current object to complete */
    } flags;
    struct {
	clientStreamNode *node;
	HttpReply *rep;
	StoreIOBuffer queuedBuffer;
    } deferredparams;
    off_t writtenToSocket;
} clientSocketContext;

CBDATA_TYPE(clientSocketContext);

/* Local functions */
/* clientSocketContext */
static FREE clientSocketContextFree;
static clientSocketContext *clientSocketContextNew(clientHttpRequest *);
/* other */
static CWCB clientWriteComplete;
static CWCB clientWriteBodyComplete;
static PF clientReadRequest;
static PF connStateFree;
static PF requestTimeout;
static PF clientLifetimeTimeout;
static void checkFailureRatio(err_type, hier_code);
static clientSocketContext *parseHttpRequestAbort(ConnStateData * conn,
    const char *uri);
static clientSocketContext *parseHttpRequest(ConnStateData *, method_t *, int *,
    char **, size_t *);
#if USE_IDENT
static IDCB clientIdentDone;
#endif
static CSCB clientSocketRecipient;
static CSD clientSocketDetach;
static void clientSetKeepaliveFlag(clientHttpRequest *);
static int clientIsContentLengthValid(request_t * r);
static DEFER httpAcceptDefer;
static int clientIsRequestBodyValid(int bodyLength);
static int clientIsRequestBodyTooLargeForPolicy(int bodyLength);
static void clientProcessBody(ConnStateData * conn);
static clientStreamNode *getTail(clientSocketContext *);
static void clientSocketRemoveThisFromConnectionList(clientSocketContext *,
    ConnStateData *);
static void clientUpdateStatHistCounters(log_type logType, int svc_time);
static void clientUpdateStatCounters(log_type logType);
static void clientUpdateHierCounters(HierarchyLogEntry *);
static int clientPingHasFinished(ping_data const *aPing);
static MemObject *clientGetMemObject(clientHttpRequest * http);
static void clientPrepareLogWithRequestDetails(request_t *, AccessLogEntry *);
static void clientLogRequest(clientHttpRequest *);
static void httpRequestFreeResources(clientHttpRequest *);
static void connEmptyOSReadBuffers(int fd);
static int connIsUsable(ConnStateData * conn);
static clientSocketContext *connGetCurrentContext(ConnStateData * conn);
static void contextDeferRecipientForLater(clientSocketContext * context, clientStreamNode * node, HttpReply * rep, StoreIOBuffer recievedData);
static int responseFinishedOrFailed(HttpReply * rep, StoreIOBuffer recievedData);
static int contextStartOfOutput(clientSocketContext * context);
static void contextSendBody(clientSocketContext * context, HttpReply * rep, StoreIOBuffer bodyData);
static void contextSendStartOfMessage(clientSocketContext * context, HttpReply * rep, StoreIOBuffer bodyData);
static void connReadNextRequest(ConnStateData * conn);
static void clientSocketContextPushDeferredIfNeeded(clientSocketContext * deferredRequest, ConnStateData * conn);
static void clientUpdateSocketStats(log_type logType, size_t size);

static clientSocketContext *clientCheckRequestLineIsParseable(char *inbuf, size_t req_sz, ConnStateData * conn);
static clientSocketContext *clientParseRequestMethod(char *inbuf, method_t * method_p, ConnStateData * conn);
static char *skipLeadingSpace(char *aString);
static char *findTrailingHTTPVersion(char *uriAndHTTPVersion);
static void trimTrailingSpaces(char *aString, size_t len);
static clientSocketContext *parseURIandHTTPVersion(char **url_p, http_version_t * http_ver_p, ConnStateData * conn);
static void setLogUri(clientHttpRequest * http, char *uri);
static void prepareInternalUrl(clientHttpRequest * http, char *url);
static void prepareForwardProxyUrl(clientHttpRequest * http, char *url);
static void prepareAcceleratedUrl(clientHttpRequest * http, char *url, char *req_hdr);
static int connGetAvailableBufferLength(ConnStateData const *conn);
static void connMakeSpaceAvailable(ConnStateData * conn);
static void connAddContextToQueue(ConnStateData * conn, clientSocketContext * context);
static int connGetConcurrentRequestCount(ConnStateData * conn);
static int connReadWasError(ConnStateData * conn, int size);
static int connFinishedWithConn(ConnStateData * conn, int size);
static void connNoteUseOfBuffer(ConnStateData * conn, int byteCount);
static int connKeepReadingIncompleteRequest(ConnStateData * conn);
static void connCancelIncompleteRequests(ConnStateData * conn);
static ConnStateData *connStateCreate(struct sockaddr_in peer, struct sockaddr_in me, int fd);
static clientStreamNode * getClientReplyContext(clientSocketContext * context);
static int connAreAllContextsForThisConnection(ConnStateData * connState);
static void connFreeAllContexts(ConnStateData * connState);
static void clientPullData(clientSocketContext * context);

clientStreamNode *
getTail(clientSocketContext * context)
{
    return context->http->client_stream.tail->data;
}

clientStreamNode *
getClientReplyContext(clientSocketContext * context)
{
    return context->http->client_stream.tail->prev->data;
}

void
clientSocketRemoveThisFromConnectionList(clientSocketContext * context,
    ConnStateData * conn)
{
    clientSocketContext **tempContextPointer;
    assert(conn);
    assert(connGetCurrentContext(conn) != NULL);
    /* Unlink us from the connection request list */
    tempContextPointer = (clientSocketContext **) & conn->currentobject;
    while (*tempContextPointer) {
	if (*tempContextPointer == context)
	    break;
	tempContextPointer = &(*tempContextPointer)->next;
    }
    assert(*tempContextPointer != NULL);
    *tempContextPointer = context->next;
    context->next = NULL;
}

void
clientSocketContextFree(void *data)
{
    clientSocketContext *context = data;
    ConnStateData *conn = context->http->conn;
    clientStreamNode *node = getTail(context);
    /* We are *always* the tail - prevent recursive free */
    assert(context == node->data);
    node->data = NULL;
    httpRequestFree(context->http);
    /* clean up connection links to us */
    assert(context != context->next);
    if (conn)
	clientSocketRemoveThisFromConnectionList(context, conn);
}

clientSocketContext *
clientSocketContextNew(clientHttpRequest * http)
{
    clientSocketContext *newContext;
    assert(http != NULL);
    CBDATA_INIT_TYPE_FREECB(clientSocketContext, clientSocketContextFree);
    newContext = cbdataAlloc(clientSocketContext);
    newContext->http = http;
    return newContext;
}

#if USE_IDENT
static void
clientIdentDone(const char *ident, void *data)
{
    ConnStateData *conn = data;
    xstrncpy(conn->rfc931, ident ? ident : dash_str, USER_IDENT_SZ);
}

#endif

void
clientUpdateStatCounters(log_type logType)
{
    statCounter.client_http.requests++;
    if (logTypeIsATcpHit(logType))
	statCounter.client_http.hits++;
    if (logType == LOG_TCP_HIT)
	statCounter.client_http.disk_hits++;
    else if (logType == LOG_TCP_MEM_HIT)
	statCounter.client_http.mem_hits++;
}

void
clientUpdateStatHistCounters(log_type logType, int svc_time)
{
    statHistCount(&statCounter.client_http.all_svc_time, svc_time);
    /*
     * The idea here is not to be complete, but to get service times
     * for only well-defined types.  For example, we don't include
     * LOG_TCP_REFRESH_FAIL_HIT because its not really a cache hit
     * (we *tried* to validate it, but failed).
     */
    switch (logType) {
    case LOG_TCP_REFRESH_HIT:
	statHistCount(&statCounter.client_http.nh_svc_time, svc_time);
	break;
    case LOG_TCP_IMS_HIT:
	statHistCount(&statCounter.client_http.nm_svc_time, svc_time);
	break;
    case LOG_TCP_HIT:
    case LOG_TCP_MEM_HIT:
    case LOG_TCP_OFFLINE_HIT:
	statHistCount(&statCounter.client_http.hit_svc_time, svc_time);
	break;
    case LOG_TCP_MISS:
    case LOG_TCP_CLIENT_REFRESH_MISS:
	statHistCount(&statCounter.client_http.miss_svc_time, svc_time);
	break;
    default:
	/* make compiler warnings go away */
	break;
    }
}

int
clientPingHasFinished(ping_data const *aPing)
{
    if (0 != aPing->stop.tv_sec && 0 != aPing->start.tv_sec)
	return -1;
    return 0;
}

void
clientUpdateHierCounters(HierarchyLogEntry * someEntry)
{
    ping_data *i;
    switch (someEntry->alg) {
    case PEER_SA_DIGEST:
	statCounter.cd.times_used++;
	break;
    case PEER_SA_ICP:
	statCounter.icp.times_used++;
	i = &someEntry->ping;
	if (clientPingHasFinished(i))
	    statHistCount(&statCounter.icp.query_svc_time,
		tvSubUsec(i->start, i->stop));
	if (i->timeout)
	    statCounter.icp.query_timeouts++;
	break;
    case PEER_SA_NETDB:
	statCounter.netdb.times_used++;
	break;
    default:
	break;
    }
}

static void
clientUpdateCounters(clientHttpRequest * http)
{
    clientUpdateStatCounters(http->logType);
    if (http->request->errType != ERR_NONE)
	statCounter.client_http.errors++;
    clientUpdateStatHistCounters(http->logType,
	tvSubMsec(http->start, current_time));
    clientUpdateHierCounters(&http->request->hier);
}

MemObject *
clientGetMemObject(clientHttpRequest * http)
{
    if (http->entry)
	return http->entry->mem_obj;
    return NULL;
}

void
clientPrepareLogWithRequestDetails(request_t * request, AccessLogEntry * aLogEntry)
{
    Packer p;
    MemBuf mb;
    assert(request);
    assert(aLogEntry);
    memBufDefInit(&mb);
    packerToMemInit(&p, &mb);
    httpHeaderPackInto(&request->header, &p);
    aLogEntry->http.method = request->method;
    aLogEntry->http.version = request->http_ver;
    aLogEntry->headers.request = xstrdup(mb.buf);
    aLogEntry->hier = request->hier;
    if (request->auth_user_request) {
	aLogEntry->cache.authuser =
	    xstrdup(authenticateUserRequestUsername(request->
		auth_user_request));
	authenticateAuthUserRequestUnlock(request->auth_user_request);
	request->auth_user_request = NULL;
    }
    packerClean(&p);
    memBufClean(&mb);
}

void
clientLogRequest(clientHttpRequest * http)
{
    if (http->out.size || http->logType) {
	http->al.icp.opcode = ICP_INVALID;
	http->al.url = http->log_uri;
	debug(33, 9) ("clientLogRequest: al.url='%s'\n", http->al.url);
	if (clientGetMemObject(http)) {
	    http->al.http.code = clientGetMemObject(http)->reply->sline.status;
	    http->al.http.content_type = strBuf(clientGetMemObject(http)->reply->content_type);
	}
	http->al.cache.caddr = http->conn ? http->conn->log_addr : no_addr;
	http->al.cache.size = http->out.size;
	http->al.cache.code = http->logType;
	http->al.cache.msec = tvSubMsec(http->start, current_time);
	if (http->request)
	    clientPrepareLogWithRequestDetails(http->request, &http->al);
	if (http->conn && http->conn->rfc931[0])
	    http->al.cache.rfc931 = http->conn->rfc931;
	accessLogLog(&http->al);
	clientUpdateCounters(http);
	if (http->conn)
	    clientdbUpdate(http->conn->peer.sin_addr, http->logType, PROTO_HTTP,
		http->out.size);
    }
}

void
httpRequestFreeResources(clientHttpRequest * http)
{
    safe_free(http->uri);
    safe_free(http->log_uri);
    safe_free(http->redirect.location);
    requestUnlink(http->request);
    http->request = NULL;
    if (http->client_stream.tail)
	clientStreamAbort(http->client_stream.tail->data, http);
}

void
httpRequestFree(void *data)
{
    clientHttpRequest *http = data;
    request_t *request = NULL;
    assert(http != NULL);
    request = http->request;
    debug(33, 3) ("httpRequestFree: %s\n", http->uri);
    /* FIXME: This needs to use the stream */
    if (!clientCheckTransferDone(http)) {
	if (request && request->body_connection)
	    clientAbortBody(request);	/* abort body transter */
	/* the ICP check here was erroneous
	 * - storeReleaseRequest was always called if entry was valid 
	 */
    }
    assert(http->logType < LOG_TYPE_MAX);
    clientLogRequest(http);
    if (request)
	checkFailureRatio(request->errType, http->al.hier.code);
    httpRequestFreeResources(http);
    /* moving to the next connection is handled by the context free */
    dlinkDelete(&http->active, &ClientActiveRequests);
    cbdataFree(http);
}

int
connAreAllContextsForThisConnection(ConnStateData * connState)
{
    clientSocketContext *context;
    assert(connState != NULL);
    context = connGetCurrentContext(connState);
    while (context) {
	if (context->http->conn != connState)
	    return 0;
	context = context->next;
    }
    return -1;
}

void
connFreeAllContexts(ConnStateData * connState)
{
    clientSocketContext *context;
    while ((context = connGetCurrentContext(connState)) != NULL) {
	assert(connGetCurrentContext(connState) !=
	    connGetCurrentContext(connState)->next);
	cbdataFree(context);
    }
}

void
connEmptyOSReadBuffers(int fd)
{
#ifdef _SQUID_LINUX_
    /* prevent those nasty RST packets */
    char buf[SQUID_TCP_SO_RCVBUF];
    while (FD_READ_METHOD(fd, buf, SQUID_TCP_SO_RCVBUF) > 0);
#endif
}

/* This is a handler normally called by comm_close() */
static void
connStateFree(int fd, void *data)
{
    ConnStateData *connState = data;
    debug(33, 3) ("connStateFree: FD %d\n", fd);
    assert(connState != NULL);
    clientdbEstablished(connState->peer.sin_addr, -1);	/* decrement */
    assert(connAreAllContextsForThisConnection(connState));
    connFreeAllContexts(connState);
    if (connState->auth_user_request)
	authenticateAuthUserRequestUnlock(connState->auth_user_request);
    connState->auth_user_request = NULL;
    authenticateOnCloseConnection(connState);
    memFreeBuf(connState->in.allocatedSize, connState->in.buf);
    pconnHistCount(0, connState->nrequests);
    cbdataFree(connState);
    connEmptyOSReadBuffers(fd);
}

/*
 * clientSetKeepaliveFlag() sets request->flags.proxy_keepalive.
 * This is the client-side persistent connection flag.  We need
 * to set this relatively early in the request processing
 * to handle hacks for broken servers and clients.
 */
static void
clientSetKeepaliveFlag(clientHttpRequest * http)
{
    request_t *request = http->request;
    const HttpHeader *req_hdr = &request->header;

    debug(33, 3) ("clientSetKeepaliveFlag: http_ver = %d.%d\n",
	request->http_ver.major, request->http_ver.minor);
    debug(33, 3) ("clientSetKeepaliveFlag: method = %s\n",
	RequestMethodStr[request->method]);
    if (!Config.onoff.client_pconns)
	request->flags.proxy_keepalive = 0;
    else {
	http_version_t http_ver;
	httpBuildVersion(&http_ver, 1, 0);
	/* we are HTTP/1.0, no matter what the client requests... */
	if (httpMsgIsPersistent(http_ver, req_hdr))
	    request->flags.proxy_keepalive = 1;
    }
}

static int
clientIsContentLengthValid(request_t * r)
{
    switch (r->method) {
    case METHOD_PUT:
    case METHOD_POST:
	/* PUT/POST requires a request entity */
	return (r->content_length >= 0);
    case METHOD_GET:
    case METHOD_HEAD:
	/* We do not want to see a request entity on GET/HEAD requests */
	return (r->content_length <= 0);
    default:
	/* For other types of requests we don't care */
	return 1;
    }
    /* NOT REACHED */
}

int
clientIsRequestBodyValid(int bodyLength)
{
    if (bodyLength >= 0)
	return 1;
    return 0;
}

int
clientIsRequestBodyTooLargeForPolicy(int bodyLength)
{
    if (Config.maxRequestBodySize &&
	bodyLength > Config.maxRequestBodySize)
	return 1;		/* too large */
    return 0;
}

int
connIsUsable(ConnStateData * conn)
{
    if (!conn || conn->fd == -1)
	return 0;
    return 1;
}

clientSocketContext *
connGetCurrentContext(ConnStateData * conn)
{
    assert(conn);
    return conn->currentobject;
}

void
contextDeferRecipientForLater(clientSocketContext * context, clientStreamNode * node, HttpReply * rep, StoreIOBuffer recievedData)
{
    debug(33, 2) ("clientSocketRecipient: Deferring %s\n", context->http->uri);
    assert(context->flags.deferred == 0);
    context->flags.deferred = 1;
    context->deferredparams.node = node;
    context->deferredparams.rep = rep;
    context->deferredparams.queuedBuffer = recievedData;
    return;
}

int
responseFinishedOrFailed(HttpReply * rep, StoreIOBuffer recievedData)
{
    if (rep == NULL && recievedData.data == NULL && recievedData.length == 0)
	return 1;
    return 0;
}

int
contextStartOfOutput(clientSocketContext * context)
{
    return context->http->out.offset == 0 ? 1 : 0;
}

void
contextSendBody(clientSocketContext * context, HttpReply * rep, StoreIOBuffer bodyData)
{
    assert(rep == NULL);
    context->http->out.offset += bodyData.length;
    comm_write(context->http->conn->fd, bodyData.data, bodyData.length,
	clientWriteBodyComplete, context, NULL);
    return;
}

void
contextSendStartOfMessage(clientSocketContext * context, HttpReply * rep, StoreIOBuffer bodyData)
{
    MemBuf mb;
    /* write headers and/or body if any */
    assert(rep || (bodyData.data && bodyData.length));
    /* init mb; put status line and headers if any */
    if (rep) {
	mb = httpReplyPack(rep);
#if HEADERS_LOG
	headersLog(0, 0, context->http->request->method, rep);
#endif
	httpReplyDestroy(rep);
	rep = NULL;
    } else {
	memBufDefInit(&mb);
    }
    if (bodyData.data && bodyData.length) {
	context->http->out.offset += bodyData.length;
	memBufAppend(&mb, bodyData.data, bodyData.length);
    }
    /* write */
    comm_write_mbuf(context->http->conn->fd, mb, clientWriteComplete, context);
    /* if we don't do it, who will? */
}

/*
 * Write a chunk of data to a client socket. If the reply is present, send the reply headers down the wire too,
 * and clean them up when finished.
 * Pre-condition: 
 *   The request is one backed by a connection, not an internal request.
 *   data context is not NULL
 *   There are no more entries in the stream chain.
 */
static void
clientSocketRecipient(clientStreamNode * node, clientHttpRequest * http,
    HttpReply * rep, StoreIOBuffer recievedData)
{
    int fd;
    clientSocketContext *context;
    /* Test preconditions */
    assert(node != NULL);
    /* TODO: handle this rather than asserting 
     * - it should only ever happen if we cause an abort and 
     * the callback chain loops back to here, so we can simply return. 
     * However, that itself shouldn't happen, so it stays as an assert for now. 
     */
    assert(cbdataReferenceValid(node));
    assert(node->data != NULL);
    assert(node->node.next == NULL);
    context = node->data;
    assert(connIsUsable(http->conn));
    fd = http->conn->fd;
    if (connGetCurrentContext(http->conn) != context) {
	contextDeferRecipientForLater(context, node, rep, recievedData);
	return;
    }
    if (responseFinishedOrFailed(rep, recievedData)) {
	clientWriteComplete(fd, NULL, 0, COMM_OK, context);
	return;
    }
    if (!contextStartOfOutput(context))
	contextSendBody(context, rep, recievedData);
    else
	contextSendStartOfMessage(context, rep, recievedData);
}

/* Called when a downstream node is no longer interested in 
 * our data. As we are a terminal node, this means on aborts
 * only
 */
void
clientSocketDetach(clientStreamNode * node, clientHttpRequest * http)
{
    clientSocketContext *context;
    /* Test preconditions */
    assert(node != NULL);
    /* TODO: handle this rather than asserting 
     * - it should only ever happen if we cause an abort and 
     * the callback chain loops back to here, so we can simply return. 
     * However, that itself shouldn't happen, so it stays as an assert for now.
     */
    assert(cbdataReferenceValid(node));
    /* Set null by ContextFree */
    assert(node->data == NULL);
    assert(node->node.next == NULL);
    context = node->data;
    /* We are only called when the client socket shutsdown.
     * Tell the prev pipeline member we're finished
     */
    clientStreamDetach(node, http);
}

static void
clientWriteBodyComplete(int fd, char *buf, size_t size, int errflag, void *data)
{
    clientWriteComplete(fd, NULL, size, errflag, data);
}

void
connReadNextRequest(ConnStateData * conn)
{
    debug(33, 5) ("clientReadNextRequest: FD %d reading next req\n",
	conn->fd);
    fd_note(conn->fd, "Waiting for next request");
    /*
     * Set the timeout BEFORE calling clientReadRequest().
     */
    commSetTimeout(conn->fd, Config.Timeout.persistent_request,
	requestTimeout, conn);
    /*
     * CYGWIN has a problem and is blocking on read() requests when there
     * is no data present.
     * This hack may hit performance a little, but it's better than 
     * blocking!.
     */
    conn->defer.until = 0;	/* Kick it to read a new request */
#ifdef _SQUID_CYGWIN_
    commSetSelect(conn->fd, COMM_SELECT_READ, clientReadRequest, conn, 0);
#else
    clientReadRequest(conn->fd, conn);	/* Read next request */
#endif
    /*
     * Note, the FD may be closed at this point.
     */
}

void
clientSocketContextPushDeferredIfNeeded(clientSocketContext * deferredRequest, ConnStateData * conn)
{
    debug(33, 2) ("clientSocketContextPushDeferredIfNeeded: FD %d Sending next\n",
	conn->fd);
    /* If the client stream is waiting on a socket write to occur, then */
    if (deferredRequest->flags.deferred) {
	/* NO data is allowed to have been sent */
	assert(deferredRequest->http->out.size == 0);
	clientSocketRecipient(deferredRequest->deferredparams.node,
	    deferredRequest->http,
	    deferredRequest->deferredparams.rep,
	    deferredRequest->deferredparams.queuedBuffer);
    }
    /* otherwise, the request is still active in a callbacksomewhere,
     * and we are done
     */
}

static void
clientKeepaliveNextRequest(clientSocketContext * context)
{
    clientHttpRequest *http = context->http;
    ConnStateData *conn = http->conn;
    clientSocketContext *deferredRequest;

    debug(33, 3) ("clientKeepaliveNextRequest: FD %d\n", conn->fd);
    cbdataFree(context);
    if ((deferredRequest = connGetCurrentContext(conn)) == NULL)
	connReadNextRequest(conn);
    else
	clientSocketContextPushDeferredIfNeeded(deferredRequest, conn);
}

void
clientUpdateSocketStats(log_type logType, size_t size)
{
    if (size == 0)
	return;
    kb_incr(&statCounter.client_http.kbytes_out, size);
    if (logTypeIsATcpHit(logType))
	kb_incr(&statCounter.client_http.hit_kbytes_out, size);
}

void
clientPullData(clientSocketContext * context)
{
    /* More data will be coming from the stream. */
    StoreIOBuffer readBuffer = EMPTYIOBUFFER;
    readBuffer.offset = context->http->out.offset;
    readBuffer.length = HTTP_REQBUF_SZ;
    readBuffer.data = context->reqbuf;
    clientStreamRead(getTail(context), context->http, readBuffer);
}

/* A write has just completed to the client, or we have just realised there is
 * no more data to send.
 */
static void
clientWriteComplete(int fd, char *bufnotused, size_t size, int errflag, void *data)
{
    clientSocketContext *context = data;
    clientHttpRequest *http = context->http;
    StoreEntry *entry = http->entry;
    clientStreamNode *node = getTail(context);
    http->out.size += size;
    assert(fd > -1);
    debug(33, 5) ("clientWriteComplete: FD %d, sz %ld, err %d, off %ld, len %d\n",
	fd, (long int) size, errflag, (long int) http->out.size, entry ? objectLen(entry) : 0);
    clientUpdateSocketStats(http->logType, size);
    if (errflag || clientHttpRequestStatus(fd, http)) {
	comm_close(fd);
	/* Do we leak here ? */
	return;
    }
    switch (clientStreamStatus(node, http)) {
    case STREAM_NONE:
	clientPullData(context);
	break;
    case STREAM_COMPLETE:
	debug(33, 5) ("clientWriteComplete: FD %d Keeping Alive\n", fd);
	clientKeepaliveNextRequest(context);
	return;
    case STREAM_UNPLANNED_COMPLETE:
	/* fallthrough */
    case STREAM_FAILED:
	comm_close(fd);
	return;
    default:
	fatal("Hit unreachable code in clientWriteComplete\n");
    }
}

extern CSR clientGetMoreData;
extern CSS clientReplyStatus;
extern CSD clientReplyDetach;

static clientSocketContext *
parseHttpRequestAbort(ConnStateData * conn, const char *uri)
{
    clientHttpRequest *http;
    clientSocketContext *context;
    StoreIOBuffer tempBuffer = EMPTYIOBUFFER;
    http = cbdataAlloc(clientHttpRequest);
    http->conn = conn;
    http->start = current_time;
    http->req_sz = conn->in.notYetUsed;
    http->uri = xstrdup(uri);
    http->log_uri = xstrndup(uri, MAX_URL);
    context = clientSocketContextNew(http);
    tempBuffer.data = context->reqbuf;
    tempBuffer.length = HTTP_REQBUF_SZ;
    clientStreamInit(&http->client_stream, clientGetMoreData, clientReplyDetach,
	clientReplyStatus, clientReplyNewContext(http), clientSocketRecipient,
	clientSocketDetach, context, tempBuffer);
    dlinkAdd(http, &http->active, &ClientActiveRequests);
    return context;
}

clientSocketContext *
clientCheckRequestLineIsParseable(char *inbuf, size_t req_sz, ConnStateData * conn)
{
    if (strlen(inbuf) != req_sz) {
	debug(33, 1) ("clientCheckRequestLineIsParseable: Requestheader contains NULL characters\n");
	return parseHttpRequestAbort(conn, "error:invalid-request");
    }
    return NULL;
}

clientSocketContext *
clientParseRequestMethod(char *inbuf, method_t * method_p, ConnStateData * conn)
{
    char *mstr = NULL;
    if ((mstr = strtok(inbuf, "\t ")) == NULL) {
	debug(33, 1) ("clientParseRequestMethod: Can't get request method\n");
	return parseHttpRequestAbort(conn, "error:invalid-request-method");
    }
    *method_p = urlParseMethod(mstr);
    if (*method_p == METHOD_NONE) {
	debug(33, 1) ("clientParseRequestMethod: Unsupported method '%s'\n", mstr);
	return parseHttpRequestAbort(conn, "error:unsupported-request-method");
    }
    debug(33, 5) ("clientParseRequestMethod: Method is '%s'\n", mstr);
    return NULL;
}

char *
skipLeadingSpace(char *aString)
{
    char *result = aString;
    while (xisspace(*aString))
	++aString;
    return result;
}

char *
findTrailingHTTPVersion(char *uriAndHTTPVersion)
{
    char *token = uriAndHTTPVersion + strlen(uriAndHTTPVersion);
    assert(*token == '\0');
    while (token > uriAndHTTPVersion) {
	--token;
	if (xisspace(*token) && !strncmp(token + 1, "HTTP/", 5))
	    return token + 1;
    }
    return uriAndHTTPVersion;
}

void
trimTrailingSpaces(char *aString, size_t len)
{
    char *endPointer = aString + len;
    while (endPointer > aString && xisspace(*endPointer))
	*(endPointer--) = '\0';
}

clientSocketContext *
parseURIandHTTPVersion(char **url_p, http_version_t * http_ver_p,
    ConnStateData * conn)
{
    char *url;
    char *token;
    /* look for URL+HTTP/x.x */
    if ((url = strtok(NULL, "\n")) == NULL) {
	debug(33, 1) ("parseHttpRequest: Missing URL\n");
	return parseHttpRequestAbort(conn, "error:missing-url");
    }
    url = skipLeadingSpace(url);
    token = findTrailingHTTPVersion(url);
    trimTrailingSpaces(url, token - url - 1);

    debug(33, 5) ("parseHttpRequest: URI is '%s'\n", url);
    *url_p = url;
    if (token == NULL) {
	debug(33, 3) ("parseHttpRequest: Missing HTTP identifier\n");
#if RELAXED_HTTP_PARSER
	httpBuildVersion(http_ver_p, 0, 9);	/* wild guess */
#else
	return parseHttpRequestAbort(conn, "error:missing-http-ident");
#endif
    } else {
	if (sscanf(token + 5, "%d.%d", &http_ver_p->major,
		&http_ver_p->minor) != 2) {
	    debug(33, 3) ("parseHttpRequest: Invalid HTTP identifier.\n");
	    return parseHttpRequestAbort(conn, "error: invalid HTTP-ident");
	}
	debug(33, 6) ("parseHttpRequest: Client HTTP version %d.%d.\n",
	    http_ver_p->major, http_ver_p->minor);
    }
    return NULL;
}

/* Utility function to perform part of request parsing */
static clientSocketContext *
clientParseHttpRequestLine(char *inbuf, size_t req_sz, ConnStateData * conn,
    method_t * method_p, char **url_p, http_version_t * http_ver_p)
{
    clientSocketContext *result = NULL;
    if ((result = clientCheckRequestLineIsParseable(inbuf, req_sz, conn))
	|| (result = clientParseRequestMethod(inbuf, method_p, conn))
	|| (result = parseURIandHTTPVersion(url_p, http_ver_p, conn)))
	return result;

    return NULL;
}

void
setLogUri(clientHttpRequest * http, char *uri)
{
    if (!stringHasCntl(uri))
	http->log_uri = xstrndup(uri, MAX_URL);
    else
	http->log_uri = xstrndup(rfc1738_escape_unescaped(uri), MAX_URL);
}

void
prepareInternalUrl(clientHttpRequest * http, char *url)
{
    http->uri = xstrdup(internalLocalUri(NULL, url));
    http->flags.internal = 1;
    http->flags.accel = 1;
}

void
prepareForwardProxyUrl(clientHttpRequest * http, char *url)
{
    size_t url_sz;
    /* URL may be rewritten later, so make extra room */
    url_sz = strlen(url) + Config.appendDomainLen + 5;
    http->uri = xcalloc(url_sz, 1);
    strcpy(http->uri, url);
    http->flags.accel = 0;
}

void
prepareAcceleratedUrl(clientHttpRequest * http, char *url, char *req_hdr)
{
    size_t url_sz;
    char *t;
    /* prepend the accel prefix */
    if (opt_accel_uses_host && (t = mime_get_header(req_hdr, "Host"))) {
	int vport;
	char *q;
	const char *protocol_name = "http";
	if (vport_mode)
	    vport = (int) ntohs(http->conn->me.sin_port);
	else
	    vport = (int) Config.Accel.port;
	/* If a Host: header was specified, use it to build the URL 
	 * instead of the one in the Config file. */
	/*
	 * XXX Use of the Host: header here opens a potential
	 * security hole.  There are no checks that the Host: value
	 * corresponds to one of your servers.  It might, for example,
	 * refer to www.playboy.com.  The 'dst' and/or 'dst_domain' ACL 
	 * types should be used to prevent httpd-accelerators 
	 * handling requests for non-local servers */
	strtok(t, " /;@");
	if ((q = strchr(t, ':'))) {
	    *q++ = '\0';
	    if (vport_mode)
		vport = atoi(q);
	}
	url_sz = strlen(url) + 32 + Config.appendDomainLen + strlen(t);
	http->uri = xcalloc(url_sz, 1);

#if SSL_FORWARDING_NOT_YET_DONE
	if (Config.Sockaddr.https->s.sin_port == http->conn->me.sin_port) {
	    protocol_name = "https";
	    vport = ntohs(http->conn->me.sin_port);
	}
#endif
	snprintf(http->uri, url_sz, "%s://%s:%d%s",
	    protocol_name, t, vport, url);
    } else if (vhost_mode) {
	int vport;
	/* Put the local socket IP address as the hostname */
	url_sz = strlen(url) + 32 + Config.appendDomainLen;
	http->uri = xcalloc(url_sz, 1);
	if (vport_mode)
	    vport = (int) ntohs(http->conn->me.sin_port);
	else
	    vport = (int) Config.Accel.port;
	rewriteURIwithInterceptedDetails(url, http->uri, url_sz,
	    http->conn->me, http->conn->peer, vport);
	debug(33, 5) ("VHOST REWRITE: '%s'\n", http->uri);
    } else {
	url_sz = strlen(Config2.Accel.prefix) + strlen(url) +
	    Config.appendDomainLen + 1;
	http->uri = xcalloc(url_sz, 1);
	snprintf(http->uri, url_sz, "%s%s", Config2.Accel.prefix, url);
    }
    http->flags.accel = 1;
}

/*
 *  parseHttpRequest()
 * 
 *  Returns
 *   NULL on error or incomplete request
 *    a clientHttpRequest structure on success
 */
static clientSocketContext *
parseHttpRequest(ConnStateData * conn, method_t * method_p, int *status,
    char **prefix_p, size_t * req_line_sz_p)
{
    char *inbuf = NULL;
    char *url = NULL;
    char *req_hdr = NULL;
    http_version_t http_ver;
    char *end;
    size_t header_sz;		/* size of headers, not including first line */
    size_t prefix_sz;		/* size of whole request (req-line + headers) */
    size_t req_sz;
    clientHttpRequest *http;
    clientSocketContext *result;
    StoreIOBuffer tempBuffer = EMPTYIOBUFFER;

    /* pre-set these values to make aborting simpler */
    *prefix_p = NULL;
    *method_p = METHOD_NONE;
    *status = -1;

    if ((req_sz = headersEnd(conn->in.buf, conn->in.notYetUsed)) == 0) {
	debug(33, 5) ("Incomplete request, waiting for end of headers\n");
	*status = 0;
	return NULL;
    }
    assert(req_sz <= conn->in.notYetUsed);
    /* Use memcpy, not strdup! */
    inbuf = xmalloc(req_sz + 1);
    xmemcpy(inbuf, conn->in.buf, req_sz);
    *(inbuf + req_sz) = '\0';

    /* Is there a legitimate first line to the headers ? */
    if ((result =
	    clientParseHttpRequestLine(inbuf, req_sz, conn, method_p, &url,
		&http_ver))) {
	/* something wrong, abort */
	xfree(inbuf);
	return result;
    }
    /*
     * Process headers after request line
     * TODO: Use httpRequestParse here.
     */
    req_hdr = strtok(NULL, null_string);
    header_sz = req_sz - (req_hdr - inbuf);
    if (0 == header_sz) {
	debug(33, 3) ("parseHttpRequest: header_sz == 0\n");
	*status = 0;
	xfree(inbuf);
	return NULL;
    }
    assert(header_sz > 0);
    debug(33, 3) ("parseHttpRequest: req_hdr = {%s}\n", req_hdr);
    end = req_hdr + header_sz;
    debug(33, 3) ("parseHttpRequest: end = {%s}\n", end);

    prefix_sz = end - inbuf;
    *req_line_sz_p = req_hdr - inbuf;
    debug(33, 3) ("parseHttpRequest: prefix_sz = %d, req_line_sz = %d\n",
	(int) prefix_sz, (int) *req_line_sz_p);
    assert(prefix_sz <= conn->in.notYetUsed);

    /* Ok, all headers are received */
    http = cbdataAlloc(clientHttpRequest);
    http->http_ver = http_ver;
    http->conn = conn;
    http->start = current_time;
    http->req_sz = prefix_sz;
    result = clientSocketContextNew(http);
    tempBuffer.data = result->reqbuf;
    tempBuffer.length = HTTP_REQBUF_SZ;
    clientStreamInit(&http->client_stream, clientGetMoreData, clientReplyDetach,
	clientReplyStatus, clientReplyNewContext(http), clientSocketRecipient,
	clientSocketDetach, result, tempBuffer);
    *prefix_p = xmalloc(prefix_sz + 1);
    xmemcpy(*prefix_p, conn->in.buf, prefix_sz);
    *(*prefix_p + prefix_sz) = '\0';
    dlinkAdd(http, &http->active, &ClientActiveRequests);

    /* XXX this function is still way to long. here is a natural point for further simplification */

    debug(33, 5) ("parseHttpRequest: Request Header is\n%s\n",
	(*prefix_p) + *req_line_sz_p);
#if THIS_VIOLATES_HTTP_SPECS_ON_URL_TRANSFORMATION
    if ((t = strchr(url, '#')))	/* remove HTML anchors */
	*t = '\0';
#endif

    if (internalCheck(url))
	prepareInternalUrl(http, url);
    else if (Config2.Accel.on && *url == '/')
	prepareAcceleratedUrl(http, url, req_hdr);
    else
	prepareForwardProxyUrl(http, url);
    setLogUri(http, http->uri);
    debug(33, 5) ("parseHttpRequest: Complete request received\n");
    xfree(inbuf);
    *status = 1;
    return result;
}

static int
clientReadDefer(int fdnotused, void *data)
{
    ConnStateData *conn = data;
    if (conn->body.size_left)
	return conn->in.notYetUsed >= conn->in.allocatedSize - 1;
    else
	return conn->defer.until > squid_curtime;
}

int
connGetAvailableBufferLength(ConnStateData const *conn)
{
    return conn->in.allocatedSize - conn->in.notYetUsed;
}

void
connMakeSpaceAvailable(ConnStateData * conn)
{
    if (connGetAvailableBufferLength(conn) < 2) {
	conn->in.buf = memReallocBuf(conn->in.buf, conn->in.allocatedSize * 2, &conn->in.allocatedSize);
	debug(33, 2) ("growing request buffer: notYetUsed=%ld size=%ld\n",
	    (long) conn->in.notYetUsed, (long) conn->in.allocatedSize);
    }
}

void
connAddContextToQueue(ConnStateData * conn, clientSocketContext * context)
{
    clientSocketContext **S;
    for (S = (clientSocketContext **) & conn->currentobject; *S;
	S = &(*S)->next);
    *S = context;
    ++conn->nrequests;
}

int
connGetConcurrentRequestCount(ConnStateData * conn)
{
    int result = 0;
    clientSocketContext **T;
    for (T = (clientSocketContext **) & conn->currentobject;
	*T; T = &(*T)->next, ++result);
    return result;
}

int
connReadWasError(ConnStateData * conn, int size)
{
    if (size < 0) {
	if (!ignoreErrno(errno)) {
	    debug(50, 2) ("connReadWasError: FD %d: %s\n", conn->fd, xstrerror());
	    return 1;
	} else if (conn->in.notYetUsed == 0) {
	    debug(50, 2) ("connReadWasError: FD %d: no data to process (%s)\n",
		conn->fd, xstrerror());
	}
    }
    return 0;
}

int
connFinishedWithConn(ConnStateData * conn, int size)
{
    if (size == 0) {
	if (connGetConcurrentRequestCount(conn) == 0 && conn->in.notYetUsed == 0) {
	    /* no current or pending requests */
	    debug(33, 4) ("connFinishedWithConn: FD %d closed\n", conn->fd);
	    return 1;
	} else if (!Config.onoff.half_closed_clients) {
	    /* admin doesn't want to support half-closed client sockets */
	    debug(33, 3) ("connFinishedWithConn: FD %d aborted (half_closed_clients disabled)\n", conn->fd);
	    return 1;
	}
    }
    return 0;
}

void
connNoteUseOfBuffer(ConnStateData * conn, int byteCount)
{
    assert(byteCount > 0 && byteCount <= conn->in.notYetUsed);
    conn->in.notYetUsed -= byteCount;
    debug(33, 5) ("conn->in.notYetUsed = %d\n", (int) conn->in.notYetUsed);
    /*
     * If there is still data that will be used, 
     * move it to the beginning.
     */
    if (conn->in.notYetUsed > 0)
	xmemmove(conn->in.buf, conn->in.buf + byteCount,
	    conn->in.notYetUsed);
}

int
connKeepReadingIncompleteRequest(ConnStateData * conn)
{
    return conn->in.notYetUsed >= Config.maxRequestHeaderSize ? 0 : 1;
}

void
connCancelIncompleteRequests(ConnStateData * conn)
{
    clientSocketContext *context = parseHttpRequestAbort(conn, "error:request-too-large");
    clientStreamNode *node = getClientReplyContext(context);
    assert(!connKeepReadingIncompleteRequest(conn));
    debug(33, 1) ("Request header is too large (%d bytes)\n",
	(int) conn->in.notYetUsed);
    debug(33, 1) ("Config 'request_header_max_size'= %ld bytes.\n",
	(long int) Config.maxRequestHeaderSize);
    clientSetReplyToError(node->data, ERR_TOO_BIG,
	HTTP_REQUEST_ENTITY_TOO_LARGE, METHOD_NONE, NULL,
	&conn->peer.sin_addr, NULL, NULL, NULL);
    connAddContextToQueue(conn, context);
    clientPullData(context);
}

static void
clientReadRequest(int fd, void *data)
{
    ConnStateData *conn = data;
    int parser_return_code = 0;
    request_t *request = NULL;
    int size;
    method_t method;
    char *prefix = NULL;
    fde *F = &fd_table[fd];
    int len;
    clientSocketContext *context;
    debug(33, 4) ("clientReadRequest: FD %d: reading request...\n", fd);
    connMakeSpaceAvailable(conn);
    len = connGetAvailableBufferLength(conn) - 1;
    commSetSelect(fd, COMM_SELECT_READ, clientReadRequest, conn, 0);
    statCounter.syscalls.sock.reads++;
    /* TODO: read should callback */
    size = FD_READ_METHOD(fd, conn->in.buf + conn->in.notYetUsed, len);
    /*
     * Don't reset the timeout value here.  The timeout value will be
     * set to Config.Timeout.request by httpAccept() and
     * clientWriteComplete(), and should apply to the request as a
     * whole, not individual read() calls.  Plus, it breaks our
     * lame half-close detection
     */
    if (size > 0) {
	fd_bytes(fd, size, FD_READ);
	kb_incr(&statCounter.client_http.kbytes_in, size);
	conn->in.notYetUsed += size;
	conn->in.buf[conn->in.notYetUsed] = '\0';	/* Terminate the string */
    } else if (size == 0) {
	if (connFinishedWithConn(conn, size)) {
	    comm_close(fd);
	    return;
	}
	/* It might be half-closed, we can't tell */
	debug(33, 5) ("clientReadRequest: FD %d closed?\n", fd);
	F->flags.socket_eof = 1;
	conn->defer.until = squid_curtime + 1;
	conn->defer.n++;
	fd_note(fd, "half-closed");
	/* There is one more close check at the end, to detect aborted
	 * (partial) requests. At this point we can't tell if the request
	 * is partial.
	 */
	/* Continue to process previously read data */
    } else if (connReadWasError(conn, size)) {
	comm_close(fd);
	return;
    }
    /* Process request body if any */
    if (conn->in.notYetUsed > 0 && conn->body.callback != NULL)
	clientProcessBody(conn);
    /* Process next request */
    if (connGetConcurrentRequestCount(conn) == 0)
	fd_note(conn->fd, "Reading next request");

    while (conn->in.notYetUsed > 0 && conn->body.size_left == 0) {
	size_t req_line_sz;
	/* Skip leading ( or trail from previous request) whitespace */
	while (conn->in.notYetUsed > 0 && xisspace(conn->in.buf[0])) {
	    xmemmove(conn->in.buf, conn->in.buf + 1, conn->in.notYetUsed - 1);
	    --conn->in.notYetUsed;
	}
	conn->in.buf[conn->in.notYetUsed] = '\0';	/* Terminate the string */
	if (conn->in.notYetUsed == 0)
	    break;
	/* Limit the number of concurrent requests to 2 */
	if (connGetConcurrentRequestCount(conn) >= (Config.onoff.pipeline_prefetch ? 2 : 1)) {
	    debug(33, 3) ("clientReadRequest: FD %d max concurrent requests reached\n",
		fd);
	    debug(33, 5) ("clientReadRequest: FD %d defering new request until one is done\n",
		fd);
	    conn->defer.until = squid_curtime + 100;	/* Reset when a request is complete */
	    conn->defer.n++;
	    return;
	}
	conn->in.buf[conn->in.notYetUsed] = '\0';	/* Terminate the string */
	/* Process request */
	context = parseHttpRequest(conn,
	    &method, &parser_return_code, &prefix, &req_line_sz);
	if (!context)
	    safe_free(prefix);
	if (context) {
	    clientHttpRequest *http = context->http;
	    /* We have an initial client stream in place should it be needed */
	    /* setup our private context */
	    connNoteUseOfBuffer(conn, http->req_sz);

	    connAddContextToQueue(conn, context);
	    commSetTimeout(fd, Config.Timeout.lifetime, clientLifetimeTimeout,
		http);
	    if (parser_return_code < 0) {
		clientStreamNode *node = getClientReplyContext(context);
		debug(33, 1) ("clientReadRequest: FD %d Invalid Request\n", fd);
		clientSetReplyToError(node->data,
		    ERR_INVALID_REQ, HTTP_BAD_REQUEST, method, NULL,
		    &conn->peer.sin_addr, NULL, conn->in.buf, NULL);
		assert(context->http->out.offset == 0);
		clientPullData(context);
		safe_free(prefix);
		break;
	    }
	    if ((request = urlParse(method, http->uri)) == NULL) {
		clientStreamNode *node = getClientReplyContext(context);
		debug(33, 5) ("Invalid URL: %s\n", http->uri);
		clientSetReplyToError(node->data,
		    ERR_INVALID_URL, HTTP_BAD_REQUEST, method, http->uri,
		    &conn->peer.sin_addr, NULL, NULL, NULL);
		assert(context->http->out.offset == 0);
		clientPullData(context);
		safe_free(prefix);
		break;
	    } else {
		/* compile headers */
		/* we should skip request line! */
		if (!httpRequestParseHeader(request, prefix + req_line_sz))
		    debug(33, 1) ("Failed to parse request headers: %s\n%s\n",
			http->uri, prefix);
		/* continue anyway? */
	    }
	    request->flags.accelerated = http->flags.accel;
	    if (!http->flags.internal) {
		if (internalCheck(strBuf(request->urlpath))) {
		    if (internalHostnameIs(request->host) &&
			request->port == getMyPort()) {
			http->flags.internal = 1;
		    } else if (internalStaticCheck(strBuf(request->urlpath))) {
			xstrncpy(request->host, internalHostname(),
			    SQUIDHOSTNAMELEN);
			request->port = getMyPort();
			http->flags.internal = 1;
		    }
		}
	    }
	    /*
	     * cache the Content-length value in request_t.
	     */
	    request->content_length = httpHeaderGetInt(&request->header,
		HDR_CONTENT_LENGTH);
	    request->flags.internal = http->flags.internal;
	    safe_free(prefix);
	    safe_free(http->log_uri);
	    http->log_uri = xstrdup(urlCanonicalClean(request));
	    request->client_addr = conn->peer.sin_addr;
	    request->my_addr = conn->me.sin_addr;
	    request->my_port = ntohs(conn->me.sin_port);
	    request->http_ver = http->http_ver;
	    if (!urlCheckRequest(request) ||
		httpHeaderHas(&request->header, HDR_TRANSFER_ENCODING)) {
		clientStreamNode *node = getClientReplyContext(context);
		clientSetReplyToError(node->data, ERR_UNSUP_REQ,
		    HTTP_NOT_IMPLEMENTED, request->method, NULL,
		    &conn->peer.sin_addr, request, NULL, NULL);
		assert(context->http->out.offset == 0);
		clientPullData(context);
		break;
	    }
	    if (!clientIsContentLengthValid(request)) {
		clientStreamNode *node = getClientReplyContext(context);
		clientSetReplyToError(node->data, ERR_INVALID_REQ,
		    HTTP_LENGTH_REQUIRED, request->method, NULL,
		    &conn->peer.sin_addr, request, NULL, NULL);
		assert(context->http->out.offset == 0);
		clientPullData(context);
		break;
	    }
	    http->request = requestLink(request);
	    clientSetKeepaliveFlag(http);
	    /* Do we expect a request-body? */
	    if (request->content_length > 0) {
		conn->body.size_left = request->content_length;
		request->body_connection = conn;
		/* Is it too large? */
		if (!clientIsRequestBodyValid(request->content_length) ||
		    clientIsRequestBodyTooLargeForPolicy(request->content_length)) {
		    clientStreamNode *node = getClientReplyContext(context);
		    clientSetReplyToError(node->data, ERR_TOO_BIG,
			HTTP_REQUEST_ENTITY_TOO_LARGE, METHOD_NONE, NULL,
			&conn->peer.sin_addr, http->request, NULL, NULL);
		    assert(context->http->out.offset == 0);
		    clientPullData(context);
		    break;
		}
	    }
	    clientAccessCheck(http);
	    continue;		/* while offset > 0 && body.size_left == 0 */
	} else if (parser_return_code == 0) {
	    if (!connKeepReadingIncompleteRequest(conn))
		connCancelIncompleteRequests(conn);
	    break;
	}
    }				/* while offset > 0 && conn->body.size_left == 0 */
    /* Check if a half-closed connection was aborted in the middle */
    if (F->flags.socket_eof) {
	if (conn->in.notYetUsed != conn->body.size_left) {	/* != 0 when no request body */
	    /* Partial request received. Abort client connection! */
	    debug(33, 3) ("clientReadRequest: FD %d aborted, partial request\n",
		fd);
	    comm_close(fd);
	    return;
	}
    }
}

/* file_read like function, for reading body content */
void
clientReadBody(request_t * request, char *buf, size_t size, CBCB * callback,
    void *cbdata)
{
    ConnStateData *conn = request->body_connection;
    if (!conn) {
	debug(33, 5) ("clientReadBody: no body to read, request=%p\n", request);
	callback(buf, 0, cbdata);	/* Signal end of body */
	return;
    }
    debug(33, 2) ("clientReadBody: start fd=%d body_size=%lu in.notYetUsed=%ld cb=%p req=%p\n",
	conn->fd, (unsigned long int) conn->body.size_left,
	(long int) conn->in.notYetUsed, callback, request);
    conn->body.callback = callback;
    conn->body.cbdata = cbdata;
    conn->body.buf = buf;
    conn->body.bufsize = size;
    conn->body.request = requestLink(request);
    clientProcessBody(conn);
}

/* Called by clientReadRequest to process body content */
static void
clientProcessBody(ConnStateData * conn)
{
    int size;
    char *buf = conn->body.buf;
    void *cbdata = conn->body.cbdata;
    CBCB *callback = conn->body.callback;
    request_t *request = conn->body.request;
    /* Note: request is null while eating "aborted" transfers */
    debug(33, 2) ("clientProcessBody: start fd=%d body_size=%lu in.notYetUsed=%ld cb=%p req=%p\n",
	conn->fd, (unsigned long int) conn->body.size_left,
	(long int) conn->in.notYetUsed, callback, request);
    if (conn->in.notYetUsed) {
	/* Some sanity checks... */
	assert(conn->body.size_left > 0);
	assert(conn->in.notYetUsed > 0);
	assert(callback != NULL);
	assert(buf != NULL);
	/* How much do we have to process? */
	size = conn->in.notYetUsed;
	if (size > conn->body.size_left)	/* only process the body part */
	    size = conn->body.size_left;
	if (size > conn->body.bufsize)	/* don't copy more than requested */
	    size = conn->body.bufsize;
	xmemcpy(buf, conn->in.buf, size);
	conn->body.size_left -= size;
	/* Move any remaining data */
	conn->in.notYetUsed -= size;
	if (conn->in.notYetUsed > 0)
	    xmemmove(conn->in.buf, conn->in.buf + size, conn->in.notYetUsed);
	/* Remove request link if this is the last part of the body, as
	 * clientReadRequest automatically continues to process next request */
	if (conn->body.size_left <= 0 && request != NULL)
	    request->body_connection = NULL;
	/* Remove clientReadBody arguments (the call is completed) */
	conn->body.request = NULL;
	conn->body.callback = NULL;
	conn->body.buf = NULL;
	conn->body.bufsize = 0;
	/* Remember that we have touched the body, not restartable */
	if (request != NULL)
	    request->flags.body_sent = 1;
	/* Invoke callback function */
	callback(buf, size, cbdata);
	if (request != NULL)
	    requestUnlink(request);	/* Linked in clientReadBody */
	debug(33, 2) ("clientProcessBody: end fd=%d size=%d body_size=%lu in.notYetUsed=%ld cb=%p req=%p\n",
	    conn->fd, size, (unsigned long int) conn->body.size_left,
	    (long int) conn->in.notYetUsed, callback, request);
    }
}

/* A dummy handler that throws away a request-body */
static void
clientReadBodyAbortHandler(char *buf, size_t size, void *data)
{
    static char bodyAbortBuf[SQUID_TCP_SO_RCVBUF];
    ConnStateData *conn = (ConnStateData *) data;
    debug(33, 2) ("clientReadBodyAbortHandler: fd=%d body_size=%lu in.notYetUsed=%ld\n",
	conn->fd, (unsigned long int) conn->body.size_left,
	(long int) conn->in.notYetUsed);
    if (size != 0 && conn->body.size_left != 0) {
	debug(33, 3) ("clientReadBodyAbortHandler: fd=%d shedule next read\n",
	    conn->fd);
	conn->body.callback = clientReadBodyAbortHandler;
	conn->body.buf = bodyAbortBuf;
	conn->body.bufsize = sizeof(bodyAbortBuf);
	conn->body.cbdata = data;
    }
}

/* Abort a body request */
int
clientAbortBody(request_t * request)
{
    ConnStateData *conn = request->body_connection;
    char *buf;
    CBCB *callback;
    void *cbdata;
    request->body_connection = NULL;
    if (!conn || conn->body.size_left <= 0)
	return 0;		/* No body to abort */
    if (conn->body.callback != NULL) {
	buf = conn->body.buf;
	callback = conn->body.callback;
	cbdata = conn->body.cbdata;
	assert(request == conn->body.request);
	conn->body.buf = NULL;
	conn->body.callback = NULL;
	conn->body.cbdata = NULL;
	conn->body.request = NULL;
	callback(buf, -1, cbdata);	/* Signal abort to clientReadBody caller */
	requestUnlink(request);
    }
    clientReadBodyAbortHandler(NULL, -1, conn);		/* Install abort handler */
    /* clientProcessBody() */
    return 1;			/* Aborted */
}

/* general lifetime handler for HTTP requests */
static void
requestTimeout(int fd, void *data)
{
#if THIS_CONFUSES_PERSISTENT_CONNECTION_AWARE_BROWSERS_AND_USERS
    ConnStateData *conn = data;
    debug(33, 3) ("requestTimeout: FD %d: lifetime is expired.\n", fd);
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
	clientHttpRequest **H;
	clientStreamNode *node;
	clientHttpRequest *http =
	parseHttpRequestAbort(conn, "error:Connection%20lifetime%20expired");
	node = http->client_stream.tail->prev->data;
	clientSetReplyToError(node->data, ERR_LIFETIME_EXP,
	    HTTP_REQUEST_TIMEOUT, METHOD_NONE, "N/A", &conn->peer.sin_addr,
	    NULL, NULL, NULL);
	/* No requests can be outstanded */
	assert(conn->chr == NULL);
	/* add to the client request queue */
	for (H = &conn->chr; *H; H = &(*H)->next);
	*H = http;
	clientStreamRead(http->client_stream.tail->data, http, 0,
	    HTTP_REQBUF_SZ, context->reqbuf);
	/*
	 * if we don't close() here, we still need a timeout handler!
	 */
	commSetTimeout(fd, 30, requestTimeout, conn);
	/*
	 * Aha, but we don't want a read handler!
	 */
	commSetSelect(fd, COMM_SELECT_READ, NULL, NULL, 0);
    }
#else
    /*
     * Just close the connection to not confuse browsers
     * using persistent connections. Some browsers opens
     * an connection and then does not use it until much
     * later (presumeably because the request triggering
     * the open has already been completed on another
     * connection)
     */
    debug(33, 3) ("requestTimeout: FD %d: lifetime is expired.\n", fd);
    comm_close(fd);
#endif
}

static void
clientLifetimeTimeout(int fd, void *data)
{
    clientHttpRequest *http = data;
    ConnStateData *conn = http->conn;
    debug(33,
	1) ("WARNING: Closing client %s connection due to lifetime timeout\n",
	inet_ntoa(conn->peer.sin_addr));
    debug(33, 1) ("\t%s\n", http->uri);
    comm_close(fd);
}

static int
httpAcceptDefer(int fdunused, void *dataunused)
{
    static time_t last_warn = 0;
    if (fdNFree() >= RESERVED_FD)
	return 0;
    if (last_warn + 15 < squid_curtime) {
	debug(33, 0) ("WARNING! Your cache is running out of filedescriptors\n");
	last_warn = squid_curtime;
    }
    return 1;
}

ConnStateData *
connStateCreate(struct sockaddr_in peer, struct sockaddr_in me, int fd)
{
    ConnStateData *result = cbdataAlloc(ConnStateData);
    result->peer = peer;
    result->log_addr = peer.sin_addr;
    result->log_addr.s_addr &= Config.Addrs.client_netmask.s_addr;
    result->me = me;
    result->fd = fd;
    result->in.buf = memAllocBuf(CLIENT_REQ_BUF_SZ, &result->in.allocatedSize);
    return result;
}

/* Handle a new connection on HTTP socket. */
void
httpAccept(int sock, void *data)
{
    int *N = &incoming_sockets_accepted;
    int fd = -1;
    ConnStateData *connState = NULL;
    struct sockaddr_in peer;
    struct sockaddr_in me;
    int max = INCOMING_HTTP_MAX;
#if USE_IDENT
    static aclCheck_t identChecklist;
#endif
    commSetSelect(sock, COMM_SELECT_READ, httpAccept, NULL, 0);
    while (max-- && !httpAcceptDefer(sock, NULL)) {
	memset(&peer, '\0', sizeof(struct sockaddr_in));
	memset(&me, '\0', sizeof(struct sockaddr_in));
	if ((fd = comm_accept(sock, &peer, &me)) < 0) {
	    if (!ignoreErrno(errno))
		debug(50, 1) ("httpAccept: FD %d: accept failure: %s\n",
		    sock, xstrerror());
	    break;
	}
	debug(33, 4) ("httpAccept: FD %d: accepted\n", fd);
	connState = connStateCreate(peer, me, fd);
	comm_add_close_handler(fd, connStateFree, connState);
	if (Config.onoff.log_fqdn)
	    fqdncache_gethostbyaddr(peer.sin_addr, FQDN_LOOKUP_IF_MISS);
	commSetTimeout(fd, Config.Timeout.request, requestTimeout, connState);
#if USE_IDENT
	identChecklist.src_addr = peer.sin_addr;
	identChecklist.my_addr = me.sin_addr;
	identChecklist.my_port = ntohs(me.sin_port);
	if (aclCheckFast(Config.accessList.identLookup, &identChecklist))
	    identStart(&me, &peer, clientIdentDone, connState);
#endif
	commSetSelect(fd, COMM_SELECT_READ, clientReadRequest, connState, 0);
	commSetDefer(fd, clientReadDefer, connState);
	clientdbEstablished(peer.sin_addr, 1);
	assert(N);
	(*N)++;
    }
}

#if USE_SSL

/* negotiate an SSL connection */
static void
clientNegotiateSSL(int fd, void *data)
{
    ConnStateData *conn = data;
    X509 *client_cert;
    int ret;

    if ((ret = SSL_accept(fd_table[fd].ssl)) <= 0) {
	if (BIO_sock_should_retry(ret)) {
	    commSetSelect(fd, COMM_SELECT_READ, clientNegotiateSSL, conn, 0);
	    return;
	}
	ret = ERR_get_error();
	if (ret) {
	    debug(83, 1)
		("clientNegotiateSSL: Error negotiating SSL connection on FD %d: %s\n",
		fd, ERR_error_string(ret, NULL));
	}
	comm_close(fd);
	return;
    }
    debug(83, 5) ("clientNegotiateSSL: FD %d negotiated cipher %s\n", fd,
	SSL_get_cipher(fd_table[fd].ssl));

    client_cert = SSL_get_peer_certificate(fd_table[fd].ssl);
    if (client_cert != NULL) {
	debug(83, 5) ("clientNegotiateSSL: FD %d client certificate: subject: %s\n",
	    fd, X509_NAME_oneline(X509_get_subject_name(client_cert), 0, 0));

	debug(83, 5) ("clientNegotiateSSL: FD %d client certificate: issuer: %s\n",
	    fd, X509_NAME_oneline(X509_get_issuer_name(client_cert), 0, 0));

	X509_free(client_cert);
    } else {
	debug(83, 5) ("clientNegotiateSSL: FD %d has no certificate.\n", fd);
    }

    commSetSelect(fd, COMM_SELECT_READ, clientReadRequest, conn, 0);
}

struct _https_port_data {
    SSL_CTX *sslContext;
};
typedef struct _https_port_data https_port_data;
CBDATA_TYPE(https_port_data);

/* handle a new HTTPS connection */
static void
httpsAccept(int sock, void *data)
{
    int *N = &incoming_sockets_accepted;
    https_port_data *https_port = data;
    SSL_CTX *sslContext = https_port->sslContext;
    int fd = -1;
    ConnStateData *connState = NULL;
    struct sockaddr_in peer;
    struct sockaddr_in me;
    int max = INCOMING_HTTP_MAX;
    SSL *ssl;
    int ssl_error;
#if USE_IDENT
    static aclCheck_t identChecklist;
#endif
    commSetSelect(sock, COMM_SELECT_READ, httpsAccept, https_port, 0);
    while (max-- && !httpAcceptDefer(sock, NULL)) {
	memset(&peer, '\0', sizeof(struct sockaddr_in));
	memset(&me, '\0', sizeof(struct sockaddr_in));
	if ((fd = comm_accept(sock, &peer, &me)) < 0) {
	    if (!ignoreErrno(errno))
		debug(50, 1) ("httpsAccept: FD %d: accept failure: %s\n",
		    sock, xstrerror());
	    break;
	}
	if ((ssl = SSL_new(sslContext)) == NULL) {
	    ssl_error = ERR_get_error();
	    debug(83, 1) ("httpsAccept: Error allocating handle: %s\n",
		ERR_error_string(ssl_error, NULL));
	    break;
	}
	SSL_set_fd(ssl, fd);
	fd_table[fd].ssl = ssl;
	fd_table[fd].read_method = &ssl_read_method;
	fd_table[fd].write_method = &ssl_write_method;
	debug(50, 5) ("httpsAccept: FD %d accepted, starting SSL negotiation.\n", fd);

	connState = connStateCreate(peer, me, fd);
	/* XXX account connState->in.buf */
	comm_add_close_handler(fd, connStateFree, connState);
	if (Config.onoff.log_fqdn)
	    fqdncache_gethostbyaddr(peer.sin_addr, FQDN_LOOKUP_IF_MISS);
	commSetTimeout(fd, Config.Timeout.request, requestTimeout, connState);
#if USE_IDENT
	identChecklist.src_addr = peer.sin_addr;
	identChecklist.my_addr = me.sin_addr;
	identChecklist.my_port = ntohs(me.sin_port);
	if (aclCheckFast(Config.accessList.identLookup, &identChecklist))
	    identStart(&me, &peer, clientIdentDone, connState);
#endif
	commSetSelect(fd, COMM_SELECT_READ, clientNegotiateSSL, connState, 0);
	commSetDefer(fd, clientReadDefer, connState);
	clientdbEstablished(peer.sin_addr, 1);
	(*N)++;
    }
}

#endif /* USE_SSL */

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

static void
clientHttpConnectionsOpen(void)
{
    sockaddr_in_list *s;
    int fd;
    for (s = Config.Sockaddr.http; s; s = s->next) {
	if (MAXHTTPPORTS == NHttpSockets) {
	    debug(1, 1) ("WARNING: You have too many 'http_port' lines.\n");
	    debug(1, 1) ("         The limit is %d\n", MAXHTTPPORTS);
	    continue;
	}
	enter_suid();
	fd = comm_open(SOCK_STREAM,
	    0,
	    s->s.sin_addr,
	    ntohs(s->s.sin_port), COMM_NONBLOCKING, "HTTP Socket");
	leave_suid();
	if (fd < 0)
	    continue;
	comm_listen(fd);
	commSetSelect(fd, COMM_SELECT_READ, httpAccept, NULL, 0);
	/*
	 * We need to set a defer handler here so that we don't
	 * peg the CPU with select() when we hit the FD limit.
	 */
	commSetDefer(fd, httpAcceptDefer, NULL);
	debug(1, 1) ("Accepting HTTP connections at %s, port %d, FD %d.\n",
	    inet_ntoa(s->s.sin_addr), (int) ntohs(s->s.sin_port), fd);
	HttpSockets[NHttpSockets++] = fd;
    }
}

#if USE_SSL
static void
clientHttpsConnectionsOpen(void)
{
    https_port_list *s;
    https_port_data *https_port;
    int fd;
    for (s = Config.Sockaddr.https; s; s = s->next) {
	if (MAXHTTPPORTS == NHttpSockets) {
	    debug(1, 1) ("WARNING: You have too many 'https_port' lines.\n");
	    debug(1, 1) ("         The limit is %d\n", MAXHTTPPORTS);
	    continue;
	}
	enter_suid();
	fd = comm_open(SOCK_STREAM,
	    0,
	    s->s.sin_addr,
	    ntohs(s->s.sin_port), COMM_NONBLOCKING, "HTTPS Socket");
	leave_suid();
	if (fd < 0)
	    continue;
	CBDATA_INIT_TYPE(https_port_data);
	https_port = cbdataAlloc(https_port_data);
	https_port->sslContext =
	    sslCreateContext(s->cert, s->key, s->version, s->cipher,
	    s->options);
	comm_listen(fd);
	commSetSelect(fd, COMM_SELECT_READ, httpsAccept, https_port, 0);
	commSetDefer(fd, httpAcceptDefer, NULL);
	debug(1, 1) ("Accepting HTTPS connections at %s, port %d, FD %d.\n",
	    inet_ntoa(s->s.sin_addr), (int) ntohs(s->s.sin_port), fd);
	HttpSockets[NHttpSockets++] = fd;
    }
}

#endif

void
clientOpenListenSockets(void)
{
    clientHttpConnectionsOpen();
#if USE_SSL
    clientHttpsConnectionsOpen();
#endif
    if (NHttpSockets < 1)
	fatal("Cannot open HTTP Port");
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

int
varyEvaluateMatch(StoreEntry * entry, request_t * request)
{
    const char *vary = request->vary_headers;
    int has_vary = httpHeaderHas(&entry->mem_obj->reply->header, HDR_VARY);
#if X_ACCELERATOR_VARY
    has_vary |=
	httpHeaderHas(&entry->mem_obj->reply->header, HDR_X_ACCELERATOR_VARY);
#endif
    if (!has_vary || !entry->mem_obj->vary_headers) {
	if (vary) {
	    /* Oops... something odd is going on here.. */
	    debug(33,
		1)
		("varyEvaluateMatch: Oops. Not a Vary object on second attempt, '%s' '%s'\n",
		entry->mem_obj->url, vary);
	    safe_free(request->vary_headers);
	    return VARY_CANCEL;
	}
	if (!has_vary) {
	    /* This is not a varying object */
	    return VARY_NONE;
	}
	/* virtual "vary" object found. Calculate the vary key and
	 * continue the search
	 */
	vary = httpMakeVaryMark(request, entry->mem_obj->reply);
	if (vary) {
	    request->vary_headers = xstrdup(vary);
	    return VARY_OTHER;
	} else {
	    /* Ouch.. we cannot handle this kind of variance */
	    /* XXX This cannot really happen, but just to be complete */
	    return VARY_CANCEL;
	}
    } else {
	if (!vary) {
	    vary = httpMakeVaryMark(request, entry->mem_obj->reply);
	    if (vary)
		request->vary_headers = xstrdup(vary);
	}
	if (!vary) {
	    /* Ouch.. we cannot handle this kind of variance */
	    /* XXX This cannot really happen, but just to be complete */
	    return VARY_CANCEL;
	} else if (strcmp(vary, entry->mem_obj->vary_headers) == 0) {
	    return VARY_MATCH;
	} else {
	    /* Oops.. we have already been here and still haven't
	     * found the requested variant. Bail out
	     */
	    debug(33, 1) ("varyEvaluateMatch: Oops. Not a Vary match on second attempt, '%s' '%s'\n",
		entry->mem_obj->url, vary);
	    return VARY_CANCEL;
	}
    }
}


/*
 * $Id$
 *
 * DEBUG: section 12      HTTP Connection
 * AUTHOR: Duane Wessels
 * BUGS:   Alex Rousskov
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
#include "HttpConn.h"     /* @?@ -> structs.h */
#include "HttpRequest.h"  /* @?@ -> structs.h */
#include "HttpReply.h"    /* @?@ -> structs.h */

/* Local constants */

/* how often we check if connection is still in half-closed state (sec) */
#define HALF_CLOSE_CHECK_TIME 1

/* how long to wait if connection timeouted, but we could not close it immediately (sec) */
#define CONN_TIMEOUT_AFTER_TIMEOUT 30

/* maximum concurrency level for passive connections */
#define MAX_PASSIVE_CC_LEVEL 2

/* max buffer sizes */
/* -1 is for 0-terminating hack (see IOBuffer.c) */
#define HTTP_CONN_MAX_IN_BUF_SIZE (4096-1)
/* do not need 0-termination here (@?@ check how to grow out_buf!)*/
#define HTTP_CONN_MAX_OUT_BUF_SIZE DISK_PAGE_SIZE

/*
 * initial buffer sizes, may grow up to the limit specified above 
 * (there is no good reason to allocate 8KB to receive small HTTP requests)
 */
#define HTTP_CONN_REQ_BUF_SIZE 1024
#define HTTP_CONN_REP_BUF_SIZE DISK_PAGE_SIZE


/* Local functions */
static HttpConn *httpConnCreate(int fd);
static void httpConnClosed(int fd, void *data);
static void httpConnTimeout(int fd, void *data);
static void httpConnDestroy(HttpConn *conn);
static void httpConnSetAddr(HttpConn *conn, struct sockaddr_in peer, struct sockaddr_in me);
static int httpConnIsDeferred(int fdnotused, void *data);
static void httpConnConnectDone(int fd, int status, void *data);
static void httpConnDefer(HttpConn *conn, time_t delta);
static void httpConnReadSocket(int fd, void *data);
static void httpConnPushData(HttpConn *conn);
static void httpConnWriteSocket(int fd, void *data);
static void httpConnPullData(HttpConn *conn);
static void httpConnBroadcastException(HttpConn *conn, int status);
static void httpConnNoteException(HttpConn *conn, int status) ;
static HttpMsg *httpConnGetNewReader(HttpConn *conn);
static HttpMsg *httpConnGetNewWriter(HttpConn *conn);
static int httpConnCanAdmitReqReader(HttpConn *conn);
static HttpMsg *httpConnGetReqReader(HttpConn *conn);
static HttpMsg *httpConnGetRepWriter(HttpConn *conn);
static void httpConnAccessCheck(HttpConn *conn);
static HttpMsg *httpConnGetReqWriter(HttpConn *conn);
static HttpMsg *httpConnGetRepReader(HttpConn *conn);


/* Create a passive http connection */
HttpConn *
httpConnAccept(int sock)
{
    int fd = -1;
    HttpConn *conn = NULL;
    struct sockaddr_in peer;
    struct sockaddr_in me;
    /* accept */
    memset(&peer, '\0', sizeof(peer));
    memset(&me, '\0', sizeof(me));
    if ((fd = comm_accept(sock, &peer, &me)) < 0)
	return NULL;
    /* create httpConn */
    debug(12, 4) ("httpConnAccept: FD %d: accepted\n", fd);
    conn = httpConnCreate(fd);
    ioBufferInit(&conn->in_buf, HTTP_CONN_REQ_BUF_SIZE-1, 1); /* enable 0-termination hack */
    ioBufferInit(&conn->out_buf, HTTP_CONN_REP_BUF_SIZE, 0);  /* disable 0-termination hack */
    /* set custom fields */
    httpConnSetAddr(conn, peer, me);
    conn->host = xstrdup(inet_ntoa(peer.sin_addr));
    conn->port = peer.sin_port;
    conn->get_reader = httpConnGetReqReader;
    conn->get_writer = httpConnGetRepWriter;
    /* start fqdn lookup if needed */
    if (Config.onoff.log_fqdn)
	fqdncache_gethostbyaddr(peer.sin_addr, FQDN_LOOKUP_IF_MISS);
    conn->ident.fd = -1;
    /* register timeout handler */
    commSetTimeout(fd, Config.Timeout.request, httpConnTimeout, conn);
    /* prepare to accept request */
    commSetSelect(fd, COMM_SELECT_READ, httpConnReadSocket, conn, 0);
    /* we must create a reader to have somebody report a timeout on start */
    httpConnGetNewReader(conn);
    return conn;
}

/* create an active http connection */
HttpConn *
httpConnConnect(const char *host, int port, const char *label)
{
    int fd = -1;
    HttpConn *conn = NULL;

    fd = comm_open(SOCK_STREAM, 0,
	Config.Addrs.tcp_outgoing,
	0, COMM_NONBLOCKING, label);
    if (fd < 0)
	return NULL;

    conn = httpConnCreate(fd);
    ioBufferInit(&conn->in_buf, HTTP_CONN_REP_BUF_SIZE-1, 1); /* enable 0-termination hack */
    ioBufferInit(&conn->out_buf, HTTP_CONN_REQ_BUF_SIZE, 0);  /* disable 0-termination hack */
    /* set custom fields */
    conn->host = xstrdup(host);
    conn->port = port;
    conn->get_reader = httpConnGetRepReader;
    conn->get_writer = httpConnGetReqWriter;
    /* note: we cannot call SetAddr here because we need to do DNS lookup first */
    /* set connect timeout @?@ maybe use different handler here? */
    commSetTimeout(fd, Config.Timeout.connect, httpConnTimeout, conn);
    /* start connecting */
    commConnectStart(fd, host, port, httpConnConnectDone, conn);
    /* note that we do not start writer -- somebody should submit it */
    return conn;
    /* @?@ @?@ we did not set peer enywere, do it: we need it in peerCheckConnectStart! */
}

/* finish creating an active http connection */
static void
httpConnConnectDone(int fd, int status, void *data)
{
    HttpConn *conn = data;
    assert(conn);

    /* @?@ would be nice to set addreses here, but comm.c does not return them! */
    /* httpConnSetAddr(conn, perr??, me??); */

    /* handle exceptions */
    if (status != COMM_OK) {
	httpConnNoteException(conn, status);
	if (conn->peer && status != COMM_ERR_DNS)
	    peerCheckConnectStart(conn->peer);
	comm_close(fd); /* we cannot continue */
    } else {
	fd_table[fd].uses++;
	/* @?@ do we need to set it here or on addWriter? */
	commSetSelect(fd, COMM_SELECT_WRITE, httpConnWriteSocket, conn, 0);
        /* @?@ should set new timeout somewhere? */
    }
    return;
}

/*
 * low level creation routine; called from httpConnAccept and httpConnConnect to
 * set common fields, register connClosed as connection close() handler,
 * register clientReadDefer defer handler
 */
static HttpConn *
httpConnCreate(int fd)
{
    HttpConn *conn;
    conn = memAllocate(MEM_HTTPCONN, 1);
    conn->fd = fd;
    conn->ident.fd = -1;
    conn->host = NULL;
    /* check if we need this @?@ */
    cbdataAdd(conn, MEM_NONE);
    comm_add_close_handler(fd, httpConnClosed, conn);
    commSetDefer(fd, httpConnIsDeferred, conn);
    return conn;
}

/* destroys connection and its dependents @?@ check if we want to destroy dependents here! */
static void
httpConnDestroy(HttpConn *conn)
{
    HttpMsg *dep;
    ObjIndexPos pos = ObjIndexInitPos;

    assert(conn);
    
    /* destroy dependents */
    while (objIndexGet(&conn -> deps, &dep, NULL, &pos))
	dep -> destroy(dep);
    objIndexClean(&conn -> deps);
    /* destroy buffers */
    ioBufferClean(&conn->in_buf);
    ioBufferClean(&conn->out_buf);
    /* destroy other dynamic fields */
    safe_free(conn->host);
    /* how do we free it? @?@ */
    memFree(MEM_HTTPCONN, conn); /* OR cbdataFree(conn); ? */
}

/* peer in active and passive connections differ?! @?@ */
static void
httpConnSetAddr(HttpConn *conn, struct sockaddr_in peer, struct sockaddr_in me)
{
    assert(conn);
    conn->addr.peer = peer;
    conn->addr.me = me;
    safe_free(conn->host);
    conn->host = xstrdup(inet_ntoa(peer.sin_addr));
    conn->port = peer.sin_port;
    conn->addr.log = peer.sin_addr;
    conn->addr.log.s_addr &= Config.Addrs.client_netmask.s_addr;
}

/* This is a handler normally called by comm_close() */
static void
httpConnClosed(int fd, void *data)
{
    HttpConn *conn = data;
    HttpMsg *dep;
    ObjIndexPos pos = ObjIndexInitPos;

    debug(12, 3) ("httpConnClosed: FD %d\n", fd);
    assert(conn);
    /* notify dependents */
    while (objIndexGet(&conn -> deps, &dep, NULL, &pos))
	dep -> noteConnClosed(dep);
    if (conn->ident.fd > -1)
	comm_close(conn->ident.fd);
    pconnHistCount(0, conn->req_count);
    httpConnDestroy(conn);
}

/* tells comm module if the connection is currently deferred */
static int
httpConnIsDeferred(int fdnotused, void *data)
{
    const HttpConn *conn = data;
    return conn->defer.until > squid_curtime;
}

/* defers the connection activity for a specified time */
static void
httpConnDefer(HttpConn *conn, time_t delta)
{
    assert(conn);
    conn->defer.until = squid_curtime + delta;
}

/*
 * reads from socket; uses configured functions to generate Readers and test
 * their ability to accept data. It is Reader's responsibility to register its
 * current state with the connection using well-known interface functions.
 */
static void
httpConnReadSocket(int fd, void *data)
{
    HttpConn *conn = data;
    int idleConnection = 0;
    IOBuffer *buf = 0;
    ssize_t size = 0;

    assert(conn);
    buf = &conn->out_buf;
    fd = conn->fd;
    assert(buf);

    debug(12, 4) ("httpConnRead: FD %d: reading...\n", fd);

    /* always maintain registered status; @?@: is it OK to do it before read? */
    commSetSelect(fd, COMM_SELECT_READ, httpConnReadSocket, conn, 0);
    if (ioBufferIsFull(buf) && buf->capacity < HTTP_CONN_MAX_IN_BUF_SIZE)
	ioBufferGrow(buf, conn, HTTP_CONN_MAX_IN_BUF_SIZE);
    if (!ioBufferIsFull(buf)) {
    	size = ioBufferRead(buf, fd, conn);
    	idleConnection = size <= 0 && objIndexIsEmpty(&conn -> deps);
    	/* handle exceptions */
    	if (size == 0) {
    	    /*
    	     * fd is closed for read, connection may still be open (half-closed);
    	     * close connection only if no pending res
    	     */
    	    if (idleConnection) {
    	    	comm_close(fd);
    	    	return; /* must exit after close */
    	    }
    	    debug(12, 5) ("httpConnRead: FD %d closed?\n", fd);
    	    /* EBIT_SET(F->flags, FD_SOCKET_EOF); @?@ not used anyway */
    	    /* defer next check for a while */
    	    httpConnDefer(conn, HALF_CLOSE_CHECK_TIME);
    	    fd_note(fd, "half-closed");
    	    return; /* exit, we did not change anything */
    	} else
    	if (size < 0) {
    	    /* close if a serious problem */
    	    if (!ignoreErrno(errno)) {
    	    	debug(12, 2) ("httpConnRead: FD %d: %s\n", fd, xstrerror());
    	    	comm_close(fd);
    	    	return; /* must exit after close */
    	    }
    	    return; /* exit, we did not change anything */
    	}
    }
    /* notify pending reader about new data */
    httpConnPushData(conn);
}

/*
 * pushes data towards pending reader; gets called whenever we may have new data
 * in the buffer OR new pending reader
 */
static void
httpConnPushData(HttpConn *conn)
{
    assert(conn);
    if (!ioBufferIsEmpty(&conn->in_buf)) { /* we have data */
	if (conn->reader || /* we have a reader OR */
	    (httpConnGetNewReader(conn) &&  /* we got one AND */
	     !ioBufferIsEmpty(&conn->in_buf))) /* the data is still there */
	    conn->reader->noteConnDataReady(conn->reader);
    }
}

/*
 * write to socket pulling data from pending Writers
 */
static void
httpConnWriteSocket(int fd, void *data)
{
    IOBuffer *buf;
    HttpConn *conn = data;
    ssize_t size;

    debug(12, 4) ("httpConnWrite: FD %d: writing...\n", fd);

    assert(conn);
    buf = &conn->out_buf;
    fd = conn->fd;
    assert(buf);

    /* register our interest to write more @?@ may not need it if no writers left? */
    commSetSelect(fd, COMM_SELECT_WRITE, httpConnWriteSocket, conn, 0);
    /* write data from the buffer (if any) to the socket */
    if (!ioBufferIsEmpty(buf)) { /* @?@ check if we must call write with empty bufffer */
	size = ioBufferWrite(buf, fd, conn);
    	/* handle exceptions */
    	if (size == 0) {
    	    /*
    	     * fd is closed for write, connection may still be open (half-closed);
    	     * not sure what to do here! @?@
    	     */
    	    if (objIndexIsEmpty(&conn->deps)) {
    	    	comm_close(fd);
    	    	return; /* must exit after close */
    	    }
    	    debug(12, 5) ("httpConnWrite: FD %d closed?\n", fd);
    	    /* defer next check for a while */
    	    httpConnDefer(conn, HALF_CLOSE_CHECK_TIME);
    	    fd_note(fd, "half-closed");
    	    return; /* exit, we did not change anything */
    	} else
    	if (size < 0) {
    	    /* close if a serious problem */
    	    if (!ignoreErrno(errno)) {
    	    	debug(12, 2) ("httpConnWrite: FD %d: %s\n", fd, xstrerror());
    	    	comm_close(fd);
    	    	return; /* must exit after close */
    	    }
    	    return; /* exit, we did not change anything */
    	}
    }
    /*
     * notify pending writer about new buffer space; it maybe confusing: when
     * we write to fd we actually read from buffer, thus, we free some space
     * in the buffer
     */
    httpConnPullData(conn);
}

/*
 * pulls data from pending writer; gets called whenever we may have new free
 * space in the write buffer OR new pending reader
 */
static void
httpConnPullData(HttpConn *conn)
{
    assert(conn);
    if (!ioBufferIsFull(&conn->out_buf)) { /* we have free space */
	if (conn->writer || /* we have a writer OR */
	    (httpConnGetNewWriter(conn) &&  /* we got one AND */
	     !ioBufferIsEmpty(&conn->out_buf))) /* the data is still there */
	    conn->writer->noteSpaceReady(conn->writer);
    }
}

/* general lifetime handler for HTTP requests */

/*
 * @?@ Check this: All dependents must be informed about a timeout. If there are
 * no dependents, we have an idle persisitent connection which can be closed.
 * Connection initialization schema insures that there is always at least one
 * dependent at the beginning unless it is an initially idle connection.
 */
static void
httpConnTimeout(int fd, void *data)
{
    HttpConn *conn = data;
    assert(conn);
    conn->timeout_count++;
    if (conn->timeout_count > 1) {
    	/* we got stuck and failed to start moving again, force close */
	debug(12, 2) ("httpConnTimeout: FD %d: lifetime is expired %d times.\n", fd, conn->timeout_count);
    	comm_close(fd);
    	return;
    }
    /* notify dependents, etc. */
    httpConnNoteException(conn, COMM_TIMEOUT);
    /* now check if anybody is still here */
    if (objIndexIsEmpty(&conn->deps)) {
    	/* close idle connection */
    	comm_close(fd);
    	return;
    }
    /* if we don't close() here, we still need a timeout handler! */
    commSetTimeout(fd, CONN_TIMEOUT_AFTER_TIMEOUT, httpConnTimeout, conn);
}

/*
 * let all dependents know what happend; some of them may use this notification
 * to form a proper timeout reply
 */
static void
httpConnBroadcastException(HttpConn *conn, int status) 
{
    HttpMsg *dep;
    ObjIndexPos pos = ObjIndexInitPos;
    assert(conn);
    /*
     * Warning: we never know what happens when dependent receives notification;
     * it may, for example, remove itself from the dependent list or even
     * destroy itself.  DIndex implementation must support iteration when the
     * base list is "unstable".
     */
    while (objIndexGet(&conn -> deps, &dep, NULL, &pos))
    	dep -> noteException(dep, status);
}

static void
httpConnNoteException(HttpConn *conn, int status) 
{
    assert(conn);
    assert(status != COMM_OK);
    /* record debugging info (always) */
    switch (status) {
	case COMM_ERR_DNS:
	    debug(12, 4) ("httpConn: Unknown host: %s\n", conn->host);
	    break;
	case COMM_TIMEOUT:
	    debug(12, 4) ("httpConn: Timeout: %s:%d\n", conn->host, conn->port);
	    break;
	default:
	    debug(12, 2) ("httpConn: something went wrong (%d) with conn to %s:%d\n",
		status, conn->host, conn->port);
    }
    /* notify dependents */
    httpConnBroadcastException(conn, status);
}

/* add dependent */
void
httpConnAddDep(HttpConn *conn, HttpMsg *dep, int id) {
    assert(conn);
    assert(dep);
    objIndexAdd(&conn->deps, dep, id);
}

/* forget about dependent */
void
httpConnDelDep(HttpConn *conn, HttpMsg *dep) {
    assert(conn);
    assert(dep);
    assert(dep != conn->reader && dep != conn->writer);
    objIndexDel(&conn->deps, dep);
}


/* readers call this when they are done reading */
void
httpConnNoteReaderDone(HttpConn *conn, HttpMsg *msg) {
    assert(conn);
    assert(msg && msg == conn->reader);
    ioBufferRUnLock(&conn->in_buf, msg);
    conn->reader = NULL;
    httpConnPushData(conn);
}

static HttpMsg *
httpConnGetNewReader(HttpConn *conn)
{
    assert(!conn->reader);
    conn->reader = conn->get_reader(conn);
    if (conn->reader)
	ioBufferWLock(&conn->in_buf, conn->reader);
    return conn->reader;
}

static HttpMsg *
httpConnGetNewWriter(HttpConn *conn)
{
    assert(!conn->writer);
    conn->writer = conn->get_writer(conn);
    if (conn->writer)
	ioBufferRLock(&conn->out_buf, conn->writer);
    return conn->writer;
}

/*
 * Custom routines for passive HTTP connections (Client -> Proxy)
 */

/*
 * return TRUE if someone makes a proxy request to us and we are in httpd-accel
 * only mode
 */
static int
checkAccelOnly(HttpConn *conn, HttpRequest *req)
{
    if (!Config2.Accel.on)
	return 0;
    if (Config.onoff.accel_with_proxy)
	return 0;
    if (req->protocol == PROTO_CACHEOBJ)
	return 0;
    if (conn->accel)
	return 0;
    return 1;
}

/* returns true if new request reader can be admitted */
static int
httpConnCanAdmitReqReader(HttpConn *conn)
{
    assert(conn);
    return conn->cc_level < MAX_PASSIVE_CC_LEVEL;
}

/* creates a new httpRequest if possible */
static HttpMsg *
httpConnGetReqReader(HttpConn *conn)
{
    assert(conn);

    /*
     * we must call httpConnAccessCheck before each request can be processed,
     * and we have to do it after url is received @?@ fix this
     */
    assert(0); 

    if (httpConnCanAdmitReqReader(conn)) {
	conn->cc_level++;
	return (HttpMsg*)httpRequestCreate(conn, conn->req_count++);
    }
    return NULL;
}

/* checks if writer is available and returns it */
static HttpMsg *
httpConnGetRepWriter(HttpConn *conn)
{
    assert(conn);

    /* to preserve the order reply.request.id must match what we are waiting for */
    return objIndexMatchOut(&conn->index, conn->rep_count); /* may return NULL */
}

/* accepts a reply to a previous request */
void
httpConnSendReply(HttpConn *conn, HttpReply *rep)
{
    int id = -1;
    assert(conn);
    assert(rep);
    assert(rep->request); /* all replies must have requests */

    /* find id of the corresponding request */
    assert(objIndexFindId(&conn->deps, rep->request, &id));
    objIndexAdd(&conn->index, rep, id);
    httpConnPullData(conn); /* this will pull reply out of the table if needed */
}

static void
httpConnAccessCheck(HttpConn *conn, HttpRequest *req)
{
    const char *browser;
    if (Config.onoff.ident_lookup && conn->ident.state == IDENT_NONE) {
	identStart(-1, conn, httpConnAccessCheck);
	return;
    }
    if (checkAccelOnly(conn, req)) {
	clientAccessCheckDone(0, conn);
	return;
    }
    browser = mime_get_header(httpHeaderGetStrField(&request->headers, "User-Agent");
    http->acl_checklist = aclChecklistCreate(Config.accessList.http,
	http->request,
	conn->addr.peer.sin_addr,
	browser,
	conn->ident.ident);
    aclNBCheck(http->acl_checklist, clientAccessCheckDone, http);
}


/*
 * Custom routines for active HTTP connections (Proxy -> Server)
 */

/* checks if writer is available and returns it */
static HttpMsg *
httpConnGetReqWriter(HttpConn *conn)
{
    assert(conn);

    /* pop next request */
    return objIndexFirstOut(&conn->index); /* may return NULL */
}

/* creates a new httpReply and ties it with corresponding httpRequest */
static HttpMsg *
httpConnGetRepReader(HttpConn *conn)
{
    HttpRequest *req;
    HttpReply *rep;
    assert(conn);

    req = objIndexMatchOut(&conn->index, conn->rep_count);
    assert(req); /* if we want to read a reply, we must have the corresponding request */

    rep = httpReplyCreate(conn, req);
    conn->rep_count++;
    (HttMsg*)return rep;
}

/* accepts a request */
void
httpConnSendRequest(HttpConn *conn, HttpRequest *req)
{
    assert(conn);
    assert(req);

    objIndexAdd(&conn->index, req, conn->req_count++);
    httpConnPullData(conn); /* this will pull request out of the table if needed */
}

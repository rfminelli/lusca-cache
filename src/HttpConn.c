
/*
 * $Id$
 *
 * DEBUG: section 12      HTTP Connection
 * AUTHOR: Duane Wessels & Alex Rousskov
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

struct _HttpConn {
    IOBuffer in_buf;
    IOBuffer out_buf;

    HttpMsg *reader;        /* current reader or NULL */
    HttpMsg *writer;        /* current writer or NULL */

    CCB get_reader;         /* called when new reader is needed */
    CCB get_writer;         /* called when new writer is needed */

    int cc_level;           /* concurrency level: #concurrent xactions being processed */
    int req_count;          /* number of requests created or admitted */
    int rep_count;          /* number of replies processed */

    HttpConnIndex index;    /* keeps pending writers, searches by HttpMsg->id */
    HttpConnDIndex deps;    /* keeps all HttpMsgs that depend on this connection */
};



/* Local constants */

/* how often we check if connection is still in half-closed state (sec) */
#define HALF_CLOSE_CHECK_TIME 1

/* how long to wait if connection timeouted, but we could not close it immediately (sec) */
#define CONN_TIMEOUT_AFTER_TIMEOUT 30

/* maximum concurrency level for passive connections */
#define MAX_PASSIVE_CC_LEVEL 2

/* Local functions */
static void httpConnClosed(int fd, void *data);
static void httpConnTimeout(int fd, void *data);


/* Create a passive http connection */
HttpConn *
httpConnAccept(int sock)
{
    int fd = -1;
    HttpConn *conn = NULL;
    struct sockaddr_in peer;
    struct sockaddr_in me;
    /* accept */
    memset(&peer, '\0', sizeof(struct sockaddr_in));
    memset(&me, '\0', sizeof(struct sockaddr_in));
    if ((fd = comm_accept(sock, &peer, &me)) < 0)
	return NULL;
    /* create httpConn */
    conn = httpConnCreate(fd);
    /* set custom fields */
    httpConnSetPeer(peer, me);
    conn->read_socket = httpConnReadReqs;
    conn->write_socket = httpConnWriteReps;
    conn->start_reader = httpConnStartReadReq;
    conn->start_writer = httpConnStartWriteRep;
    /* start fqdn lookup if needed */
    if (Config.onoff.log_fqdn)
	fqdncache_gethostbyaddr(peer.sin_addr, FQDN_LOOKUP_IF_MISS);
    /* register timeout handler */
    commSetTimeout(fd, Config.Timeout.request, httpConnTimeout, conn);
    /* prepare to accept request */
    commSetSelect(fd, COMM_SELECT_READ, conn -> read_socket, conn, 0);
    /* we must create a reader to have somebody report a timeout on start */
    conn -> start_reader(conn);
    return conn;
}

/* start create an active http connection */
static HttpConn *
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
    /* set custom fields */
    conn->host = xstrdup(host);
    conn->port = port;
    conn->read_socket = httpConnReadReps;
    conn->write_socket = httpConnWriteReqs;
    conn->start_reader = httpConnStartReadRep;
    conn->start_writer = httpConnStartWriteReq;
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
    conn = xcalloc(1, sizeof(HttpConn));
    conn->fd = fd;
    conn->ident.fd = -1;
    conn->host = xstrdup("??"); /* in case we forget to set it, remove it later @?@ */
    conn->in_buf = ioBufferCreate(REQUEST_BUF_SIZE);
    cbdataAdd(conn, MEM_NONE);
    comm_add_close_handler(fd, httpConnClosed, conn);
    commSetDefer(fd, httpConnDefer, conn);
}

/* destroys connection and its dependents @?@ check if we want to destroy dependents here! */
static void
httpConnDestroy(HttpConn *conn)
{
    HttpConn *conn;
    ConnDependent *dep;
    DepListPos pos = DepListInitPos;

    assert(conn);
    
    /* destroy dependents */
    while (depListGet(conn -> dependents, &dep, &pos))
	dep -> destroy(dep);
    depListDestroy(conn -> dependents);
    /* destroy buffers */
    ioBufferFree(conn->in_buf);
    ioBufferFree(conn->out_buf);
    /* destroy other dynamic fields */
    safe_free(conn->host);
    cbdataFree(conn);
}

/* peer in active and passive connections differ?! @?@ */
static void
httpConnSetPeer(HttpConn *conn, struct sockaddr_in peer, struct sockaddr_in me)
{
    asset(conn);
    conn->peer = peer;
    safe_free(conn->host);
    conn->host = xstrdup(inet_ntoa(peer.sin_addr));
    conn->port = peer.port;
    conn->log_addr = peer.sin_addr;
    conn->log_addr.s_addr &= Config.Addrs.client_netmask.s_addr;
    conn->me = me;
}

/* This is a handler normally called by comm_close() */
static void
httpConnClosed(int fd, void *data)
{
    HttpConn *conn = data;
    clientHttpRequest *http;
    Dependent *dep;
    DepListPos pos = DepListInitPos;

    debug(12, 3) ("httpConnClosed: FD %d\n", fd);
    assert(conn);
    /* notify dependents */
    while (depListGet(conn -> dependents, &dep, &pos))
	dep -> noteConnClosed(conn);
    if (conn->ident.fd > -1)
	comm_close(conn->ident.fd);
    pconnHistCount(0, conn->req_count);
    httpConnDestroy(conn);
}

/* tells comm module if the connection is currently deferred */
static int
httpConnDefer(int fdnotused, void *data)
{
    const HttpConn *conn = data;
    return conn->defer.until > squid_curtime;
}

/*
 * reads from socket; uses configured functions to generate Readers and test
 * their ability to accept data. It is Reader's responsibility to register its
 * current state with the connection using well-known interface functions.
 */
static void
httpConnReadSocket(HttpConn *conn)
{
    int fd;
    int idleConnection = 0;
    IOBuffer *buf;

    assert(conn);
    buf = conn->out_buf;
    fd = conn->fd;
    assert(buf);


    debug(12, 4) ("httpConnRead: FD %d: reading...\n", fd);

    /* always maintain registered status; @?@: is it OK to do it before read?*/
    commSetSelect(fd, COMM_SELECT_READ, conn->readSocket, conn, 0);
    if (buf->free_space > 0) {
    	size = ioBufferRead(buf, fd, conn);
    	idleConnection = conn -> queue -> is_empty && size <= 0;
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
    	    httpConnDefer(HALF_CLOSE_CHECK_TIME);
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
    if (!ioBufferIsEmpty(conn->in_buf)) { /* we have data */
	if (!conn->reader) /* no reader to read data */
	    conn->reader = conn->get_reader(conn); /* may do nothing because of load control, etc. */
	if (!ioBufferIsEmpty(conn->in_buf) && conn->reader) /* double check */
	    conn->reader->noteDataReady(conn);
    }
}

/*
 * write to socket pulling data from pending Writers
 */
static void
httpConnWriteSocket(HttpConn *conn)
{
    int fd;
    IOBuffer *buf;

    debug(12, 4) ("httpConnWrite: FD %d: writing...\n", fd);

    assert(conn);
    buf = conn->out_buf;
    fd = conn->fd;
    assert(buf);

    /* register our interest to write more @?@ may not need it if no writers left? */
    commSetSelect(fd, COMM_SELECT_WRITE, conn->write_socket, conn, 0);
    /* write data from the buffer (if any) to the socket */
    if (buf->size) { /* @?@ check if we must call write with empty bufffer */
	size = ioBufferWrite(buf, fd, conn);
    	/* handle exceptions */
    	if (size == 0) {
    	    /*
    	     * fd is closed for write, connection may still be open (half-closed);
    	     * not sure what to do here! @?@
    	     */
    	    if (depListEmpty(conn->dependents)) {
    	    	comm_close(fd);
    	    	return; /* must exit after close */
    	    }
    	    debug(12, 5) ("httpConnWrite: FD %d closed?\n", fd);
    	    /* defer next check for a while */
    	    httpConnDefer(HALF_CLOSE_CHECK_TIME);
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
     * notify pending writer about new buffer space; it may be confusing: when
     * we write to fd we actually read from buffer(!), thus we free some space
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
    if (!ioBufferIsFull(conn->in_buf)) { /* we have free space */
	if (!conn->writer) /* no writer to write data */
	    conn->writer = conn->get_writer(conn); /* may fail because nobody is waiting, etc. */
	if (!ioBufferIsFull(conn->in_buf) && conn->writer) /* double check */
	    conn->writer->noteSpaceReady(conn);
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
    if (conn->timeout_count) {
    	/* we got stuck and failed to start moving again, force close */
	debug(12, 2) ("httpConnTimeout: FD %d: lifetime is expired %d times.\n", fd, conn->timeout_count);
    	comm_close(fd);
    	return;
    }
    /* notify dependents, etc. */
    httpConnNoteException(conn, COMM_TIMEOUT);
    /* now check if anybody is still here */
    if (depListEmpty(conn->dependents)) {
    	/* close idle connection */
    	comm_close(fd);
    	return;
    }
    /* if we don't close() here, we still need a timeout handler! */
    commSetTimeout(fd, CONN_TIMEOUT_AFTER_TIMEOUT, httpConnTimeout, conn);
    conn->timeout_count++;
}

/*
 * let all dependents know what happend; some of them may use this notification
 * to form a proper timeout reply
 */
static void
httpConnBroadcastException(HttpConn *conn, int status) 
{
    Dependent *dep;
    DepListPos pos = DepListInitPos;
    assert(conn);
    /*
     * Warning: we never know what happens when dependent receives notification;
     * it may, for example, remove itself from the dependent list or even
     * destroy itself.  DepList implementation must support iteration when the
     * base llist is "unstable".
     */
    while (depListGet(conn -> dependents, &dep, &pos))
    	dep -> noteException(conn, status);
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
	    debug(12, 4) ("httpConn: Timeout: %s:%d\n", conn->host, conn_port);
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
httpConnAddDep(HttpConn *conn, httpMsg *dep) {
    assert(conn);
    assert(dep);
    httpConnDIndexAdd(&conn->deps, dep);
}

/* forget about dependent */
void
httpConnDelDep(HttpConn *conn, httpMsg *dep) {
    assert(conn);
    assert(dep);
    assert(dep != conn->reader && dep != conn->writer);
    assert(!httpConnIndexIsMember(&conn->index, dep));
    httpConnDIndexDel(&conn->deps, dep));
}


/* readers call this when they are done reading */
void
httpConnNoteReaderDone(HttpConn *conn, httpMsg *msg) {
    assert(conn);
    assert(msg && msg == conn->reader);
    conn->reader = NULL;
    httpConnPushData(conn);
}

/*
 * Custom routines for passive HTTP connections (Client -> Proxy)
 */

/* returns true if new request reader can be admitted */
static void
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

    if (httpConnCanAdmitReq(conn)) {
	conn->cc_level++;
	return httpReqCreate(conn, conn->req_count++);
    }
    return NULL;
}

/* checks if writer is available and returns it */
static HttpMsg *
httpConnGetRepWriter(HttpConn *conn)
{
    assert(conn);

    /* to preserve the order reply.request.id must match what we are waiting for */
    return httpConnIndexMatchOut(&conn->index, conn->rep_count); /* may return NULL */
}

/* accepts a reply to a previous request */
void
httpConnSendReply(HttpConn *conn, HttpReply *rep)
{
    assert(conn);
    assert(rep);
    assert(rep->request); /* all replies must have requests */

    httpConnIndexAdd(conn->index, rep);
    httpConnPullData(conn); /* this will pull reply out of the table if needed */
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
    return httpConnIndexFirstOut(&conn->index); /* may return NULL */
}

/* creates a new httpReply and ties it with corresponding httpRequest */
static HttpMsg *
httpConnGetRepReader(HttpConn *conn)
{
    HttpRequest *req;
    HttpReply *rep;
    assert(conn);

    req = httpConnIndexMatchOut(&conn->index, conn->rep_count);
    assert(req); /* if we want to read a reply, we must have the corresponding request */

    rep = httpRepCreate(conn, req);
    conn->rep_count++;
    return rep;
}

/* accepts a request */
void
httpConnSendRequest(HttpConn *conn, HttpRequest *req)
{
    assert(conn);
    assert(req);

    httpMsgSetId(req, conn->req_count++);
    httpConnIndexAdd(&conn->index, req);
    httpConnPullData(conn); /* this will pull request out of the table if needed */
}

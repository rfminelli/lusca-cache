
/*
 * $Id$
 *
 * DEBUG: section 12      HTTP Connection
 * AUTHOR:
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

/* queue of HttpRe's
struct _HttpReQueue {
    HttpRe *head;
    HttpRe *tail;
    HttpRe *pending;
    int allow_pending_id; /* re with this id is allowed to become pending */
    int last_pending_id;  /* when re with this id is processed, the connection is closed */
};

struct _HttpConn {
    HttpReQueue read_queue;
    HttpReQueue write_queue;
    int is_passive;   /* true if we accept requests (and, thus, send replies) */
};

/* Local constants */

/* how often we check if connection is still in half-closed state (sec) */
#define HALF_CLOSE_CHECK_TIME 1

/* how long to wait if connection timeouted, but we could not close it immediately (sec) */
#define CONN_TIMEOUT_AFTER_TIMEOUT 30

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

{.. move it to ioBuffer
	ioBufferWriteOpen(conn->in_buf);
    meta_data.misc +-= conn->in.size; <-- put this into ioBufferWriteOpen/Close
	size = read(fd, conn->in_buf->write_ptr, conn->in_buf->free_space);
	if (size > 0) { 
		ioBufferWrote(conn->in_buf, size);
		fd_bytes(fd, size, FD_READ);
	}
	ioBufferWriteClose(conn->in_buf);
	return size;
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
    /* notify pending reader (if any) about new data (if any) */
    if (conn->read_queue->head && buf->size)
    	conn->read_queue->head->noteDataReady(); /* it has a pointer to buf */
    /*
     * start readers while (a) we have data in the buffer, (b) buffer is not
     * locked, (c) admission policy OKs
     */
    while (buf->size && !buf->read_lock && httpConnCanAdmitReader(conn)) {
    	conn->start_reader(conn);
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

    /* notify pending writer (if any) about new buffer space (if any) */
    if (conn->write_queue->head && buf->free_space)
	conn->write_queue->head->noteConnReady(); /* it has a pointer to out_buf */
    /*
     * start writers while (a) we have space in the buffer, (b) buffer is not
     * locked, (c) admission policy OKs
     */
    while (buf->free_space && !buf->write_lock && httpConnCanAdmitWriter(conn)) {
	conn->start_writerer(conn);
    }
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
    /* nothing to be done here? */
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


/* Accept a new connection on HTTP socket (MoveThis!) */
void
httpAccept(int sock, void *notused)
{
    /* re-register this handler */
    commSetSelect(sock, COMM_SELECT_READ, httpAccept, NULL, 0);
    if (!httpConnAccept(sock))
    	debug(12, 1) ("httpAccept: FD %d: accept failure: %s\n",
	    sock, xstrerror());
    else
	debug(12, 4) ("httpAccept: FD %d: accepted\n", fd);
}


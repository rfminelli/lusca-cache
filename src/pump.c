/*
 * $Id$
 *
 * DEBUG: section 61    PUMP handler
 * AUTHOR: Kostas Anagnostakis
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

#define PUMP_MAXBUFFER 2*SQUID_UDP_SO_SNDBUF

struct _PumpStateData {
    request_t *req;
    int c_fd;			/* client fd */
    int s_fd;			/* server end */
    int rcvd;			/* bytes received from client */
    int sent;			/* bytes sent to server */
    int body_sz;		/* should equal content-length + 2 */
    StoreEntry *request_entry;	/* the request entry */
    StoreEntry *reply_entry;	/* the reply entry */
    CWCB *callback;		/* what to do when we finish sending */
    void *cbdata;		/* callback data passed to callback func */
    int flags;
    struct _PumpStateData *next;
};

typedef struct _PumpStateData PumpStateData;

static PumpStateData *pump_head = NULL;

static PF pumpReadFromClient;
static STCB pumpServerCopy;
static CWCB pumpServerCopyComplete;
static PF pumpFree;
static PF pumpTimeout;
static PF pumpServerClosed;
static DEFER pumpReadDefer;
static void pumpClose(void *data);

void
pumpInit(int fd, request_t * r, char *uri)
{
    int flags = 0;
    LOCAL_ARRAY(char, new_key, MAX_URL + 8);
    HttpHeader hdr;
    int clen = 0;
    char *hdrStart, *hdrEnd;
    PumpStateData *pumpState = xcalloc(1, sizeof(PumpStateData));
    StoreEntry *e;
    debug(61, 3) ("pumpInit: FD=%d , uri=%s\n", fd, uri);
    /* create a StoreEntry which will buffer the data 
     * to be pumped */

    assert(fd > -1);
    assert(uri);
    assert(r);

    memset(&hdr, '\0', sizeof(HttpHeader));
    hdrStart = r->headers;
    hdrEnd = &r->headers[r->headers_sz];
    if (!httpHeaderParse(&hdr, hdrStart, hdrEnd)) {
	debug(61, 1) ("pumpInit: Cannot find Content-Length.\n");
	xfree(pumpState);
	return;
    }
    clen = httpHeaderGetInt(&hdr, HDR_CONTENT_LENGTH);
    debug(61, 4) ("pumpInit: Content-Length=%d.\n", clen);

    flags = REQ_NOCACHE;	/* for now, don't cache */
    snprintf(new_key, MAX_URL + 5, "%s|Pump", uri);
    e = storeCreateEntry(new_key, new_key, flags, r->method);

/* initialize data structure */

    pumpState->c_fd = fd;
    pumpState->s_fd = -1;
    pumpState->body_sz = clen + 2;
    pumpState->request_entry = e;
    pumpState->req = requestLink(r);
    pumpState->callback = NULL;
    pumpState->cbdata = NULL;
    pumpState->next = pump_head;
    pump_head = pumpState;
    cbdataAdd(pumpState, MEM_NONE);
    comm_add_close_handler(pumpState->c_fd, pumpFree, pumpState);
    commSetSelect(fd, COMM_SELECT_READ, NULL, NULL, 0);
    debug(61, 4) ("pumpInit: FD %d, Created %p\n", fd, pumpState);
}

void
pumpStart(int s_fd, StoreEntry * reply_entry, request_t * r, CWCB * callback, void *cbdata)
{
    PumpStateData *p = NULL;
    debug(61, 3) ("pumpStart: FD %d, key %s\n",
	s_fd, storeKeyText(reply_entry->key));

    /* find state data generated by pumpInit in linked list */
    for (p = pump_head; p && p->req != r; p = p->next);
    assert(p != NULL);
    assert(p->request_entry);
    assert(p->c_fd > -1);

    /* fill in the rest of data needed by the pump */
    p->s_fd = s_fd;
    p->reply_entry = reply_entry;
    p->callback = callback;
    p->cbdata = cbdata;
    cbdataLock(p->cbdata);
    storeLockObject(p->reply_entry);
    comm_add_close_handler(p->s_fd, pumpServerClosed, p);

    /* ok, we're ready to go, register as a client to our storeEntry */
    storeClientListAdd(p->request_entry, p);

    /* see if part of the body is in the request */
    if (r->body_sz > 0) {
	assert(r->body);
	debug(61, 0) ("pumpStart: Appending %d bytes from r->body.\n",
	    r->body_sz);
	storeAppend(p->request_entry, r->body, r->body_sz);
	p->rcvd = r->body_sz;
    }

    /* setup data pump */
    assert(p->c_fd > -1);
    commSetSelect(p->c_fd, COMM_SELECT_READ, pumpReadFromClient, p, 0);
    commSetTimeout(p->c_fd, Config.Timeout.read, pumpTimeout, p);
    commSetDefer(p->c_fd, pumpReadDefer, p);
    storeClientCopy(p->request_entry, 0, 0, 4096,
	memAllocate(MEM_4K_BUF),
	pumpServerCopy, p);
}

static void
pumpServerCopy(void *data, char *buf, ssize_t size)
{
    PumpStateData *p = data;

    debug(61, 5) ("pumpServerCopy: called with size=%d\n", size);

    if (size < 0) {
	debug(61, 5) ("pumpServerCopy: freeing and returning\n");
	memFree(MEM_4K_BUF, buf);
	pumpClose(p);
	return;
    }
    if (size == 0) {
	debug(61, 5) ("pumpServerCopy: done, finishing\n", size);
	pumpServerCopyComplete(p->s_fd, NULL, 0, DISK_OK, p);
	memFree(MEM_4K_BUF, buf);
	return;
    }
    debug(61, 5) ("pumpServerCopy: to FD %d, %d bytes\n", p->s_fd, size);

    comm_write(p->s_fd, buf, size, pumpServerCopyComplete, p, memFree4K);
}

static void
pumpServerCopyComplete(int fd, char *bufnotused, size_t size, int errflag, void *data)
{
    PumpStateData *p = data;
    StoreEntry *e = p->request_entry;
    debug(61, 5) ("pumpServerCopyComplete: called with size=%d (%d,%d)\n",
	size, p->sent + size, p->body_sz);

    if (size <= 0 || e->store_status == STORE_ABORTED) {
	debug(61, 5) ("pumpServerCopyComplete: aborted!\n", size);
	assert(p->reply_entry != NULL);
	storeAbort(p->reply_entry, 0);	/* ? --DW */
	pumpClose(p);
	return;
    }
    p->sent += size;
    assert(p->sent <= p->body_sz);
    if (p->sent == p->body_sz) {
	debug(61, 5) ("pumpServerCopyComplete: Done!\n", size);
	storeComplete(p->request_entry);
	if (cbdataValid(p->cbdata))
	    p->callback(p->s_fd, NULL, p->sent, 0, p->cbdata);
	cbdataUnlock(p->cbdata);
	/*
	 * We have no more to write to s_fd.  forget it, but don't
	 * close it
	 */
	p->s_fd = -1;
	return;
    }
    debug(61, 5) ("pumpServerCopyComplete: copying from entry at offset %d\n",
	p->sent - size);
    storeClientCopy(e, p->sent, p->sent, 4096,
	memAllocate(MEM_4K_BUF),
	pumpServerCopy, p);
}


static void
pumpReadFromClient(int fd, void *data)
{
    PumpStateData *p = data;
    LOCAL_ARRAY(char, buf, SQUID_TCP_SO_RCVBUF);
    int len = 0;
    errno = 0;
    len = read(fd, buf, SQUID_TCP_SO_RCVBUF);
    fd_bytes(fd, len, FD_READ);

    debug(61, 5) ("pumpReadFromClient: FD %d: len %d.\n", fd, len);

    if (len < 0) {
	debug(61, 2) ("pumpReadFromClient: FD %d: read failure: %s.\n",
	    fd, xstrerror());
	if (ignoreErrno(errno)) {
	    debug(61, 5) ("pumpReadFromClient: FD %d: len %d and ignore!\n",
		fd, len);
	    commSetSelect(fd,
		COMM_SELECT_READ,
		pumpReadFromClient,
		p,
		Config.Timeout.read);
	} else {
	    debug(61, 2) ("pumpReadFromClient: aborted.\n");
	    storeAbort(p->request_entry, 0);
	}
    } else if (len == 0 && p->request_entry->mem_obj->inmem_hi == 0) {
	debug(61, 2) ("pumpReadFromClient: FD %d: failed.\n", fd);
	storeAbort(p->request_entry, 0);
    } else if (len == 0) {
	/* connection closed, call close handler */
	if (p->rcvd >= p->body_sz) {
	    if (p->rcvd > p->body_sz)
		debug(61, 1) ("pumpReadFromClient: Warning: rcvd=%d, body_sz=%d\n", p->rcvd, p->body_sz);
	    debug(61, 2) ("pumpReadFromClient: finished!\n");
	    commSetSelect(p->c_fd, COMM_SELECT_READ, NULL, NULL, 0);
	    storeComplete(p->request_entry);
	} else {
	    debug(61, 4) ("pumpReadFromClient: incomplete request or connection broken.\n");
	    storeAbort(p->request_entry, 0);
	}
    } else {
	/* ok, go ahead */
	storeAppend(p->request_entry, buf, len);
	p->rcvd += len;
	commSetSelect(fd,
	    COMM_SELECT_READ,
	    pumpReadFromClient,
	    p,
	    Config.Timeout.read);
    }
}

static int
pumpReadDefer(int fd, void *data)
{
    PumpStateData *p = data;
    debug(61, 9) ("pumpReadDefer: FD %d, Rcvd %d bytes, Sent %d bytes\n",
	fd, p->rcvd, p->sent);
    assert(p->rcvd >= p->sent);
    if ((p->rcvd - p->sent) < PUMP_MAXBUFFER)
	return 0;
    debug(61, 9) ("pumpReadDefer: deferring, rcvd=%d, sent=%d\n",
	p->rcvd, p->sent);
    return 1;
}

static void
pumpClose(void *data)
{
    PumpStateData *p = data;
    debug(0, 0) ("pumpClose: %p Server FD %d, Client FD %d\n", p, p->s_fd, p->c_fd);
    if (p->s_fd > -1) {
	comm_remove_close_handler(p->s_fd, pumpServerClosed, p);
	comm_close(p->s_fd);
	p->s_fd = -1;
    }
    if (p->c_fd > -1) {
	comm_close(p->c_fd);
    }
}

static void
pumpFree(int fd, void *data)
{
    PumpStateData *p = NULL;
    PumpStateData *q = NULL;
    debug(61, 2) ("pumpFree: FD %d, releasing %p!\n", fd, data);
    for (p = pump_head; p && p != data; q = p, p = p->next);
    if (p == NULL) {
	debug(61, 1) ("pumpFree: p=%p not found?\n", p);
	return;
    }
    if (q)
	q->next = p->next;
    else
	pump_head = p->next;
    assert(fd == p->c_fd);
    if (p->request_entry->store_status == STORE_PENDING)
	storeAbort(p->reply_entry, 0);	/* ? --DW */
    storeUnregister(p->request_entry, p);
    storeUnlockObject(p->request_entry);
    if (p->reply_entry != NULL)
	storeUnlockObject(p->reply_entry);
    requestUnlink(p->req);
    cbdataFree(p);
}

static void
pumpTimeout(int fd, void *data)
{
    PumpStateData *p = data;
    debug(0, 0) ("pumpTimeout: FD %d\n", p->c_fd);
    /* XXX need more here */
    pumpClose(p);
}

/*
 *This is called only if the client connect closes unexpectedly
 */
static void
pumpServerClosed(int fd, void *data)
{
    PumpStateData *p = data;
    debug(61, 3) ("pumpServerClosed: FD %d\n", fd);
    /*
     * we have been called from comm_close for the server side, so
     * just need to clean up the client side
     */
    /* XXX I'm sure we need to do something here */
    comm_close(p->c_fd);
}

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
    int cont_len;		/* Content-length, if present */
    StoreEntry *buf_entry;	/* our pump entry */
    StoreEntry *reply_entry;	/* the reply entry */
    void *cbdata;		/* callback data passed to callback func */
    void *callback;		/* what to do when we finish sending */
    int flags;
    struct _PumpStateData *next;
};

enum {
    CLOSEHDL_SET
};

typedef struct _PumpStateData PumpStateData;

static PumpStateData *pump_head = NULL;

static PF pumpReadFromClient;
static STCB pumpServerCopy;
static CWCB pumpServerCopyComplete;
static PF pumpFree;
static DEFER pumpReadDefer;

void
pumpInit(int fd, request_t * r, char *uri)
{
    int flags = 0;
    LOCAL_ARRAY(char, new_key, MAX_URL + 8);
    HttpHeader hdr;
    int clen=0;
    char *hdrStart, *hdrEnd;
    PumpStateData *pumpState = xcalloc(1, sizeof(PumpStateData));
    StoreEntry *e;
    debug(61, 3) ("pumpInit: FD=%d , uri=%s\n", fd, uri);
    /* create a StoreEntry which will buffer the data 
     * to be pumped */

    assert(uri);
    assert(r);

    memset(&hdr, '\0' , sizeof(HttpHeader));
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
    pumpState->cont_len = clen;
    pumpState->buf_entry = e;
    pumpState->req = requestLink(r);
    pumpState->callback = NULL;
    pumpState->cbdata = NULL;
    pumpState->next = pump_head;
    pump_head = pumpState;
    cbdataAdd(pumpState, MEM_NONE);
    comm_add_close_handler(pumpState->c_fd, pumpFree, pumpState);
    EBIT_SET(pumpState->flags, CLOSEHDL_SET);

    if (fd>=0)
    	commSetSelect(fd, COMM_SELECT_READ, NULL, NULL, 0);
    debug(61, 4) ("pumpInit: Created %p.\n", pumpState);

}

void
pumpStart(int s_fd, StoreEntry * reply_entry, request_t * r, void *callback, void *cbdata)
{
    PumpStateData *p = NULL;
    debug(61, 0) ("pumpStart: called: FD=%d callback=%x\n", s_fd, r);

/* find state data generated by pumpInit in linked list */

    for (p = pump_head; p  && p->req != r; p = p->next);

    if (!p) {
	debug(61, 0) ("pumpStart: called for unknown req %x\n", r);
	exit(-1);
    }

    assert(p->buf_entry);
    assert(p->c_fd > 0);

/* fill in the rest of data needed by the pump */

    p->s_fd = s_fd;
    p->reply_entry = reply_entry;
    p->callback = callback;
    p->cbdata = cbdata;

/* ok, we're ready to go, register as a client to our storeEntry */

    storeLockObject(p->buf_entry);
    storeClientListAdd(p->buf_entry, p);

    comm_add_close_handler(p->s_fd, pumpFree, p);

/* see if part of the body is in the request */

    if (r->body_sz != 0) {
	assert(r->body);
	debug(61, 0) ("pumpStart: Appending %d bytes from r->body.\n",
	    r->body_sz);
	storeAppend(p->buf_entry, r->body, r->body_sz);
	p->rcvd=r->body_sz-2;
    }

/* setup data pump */

    assert(p->c_fd > 0);
    commSetSelect(p->c_fd, COMM_SELECT_READ, pumpReadFromClient, p, 10);

    storeClientCopy(p->buf_entry, 0, 0, 4096,
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
	pumpFree(p->s_fd, p->req);
	return;
    }
    if (size == 0) {
	debug(61, 5) ("pumpServerCopy: done, finishing\n", size);
	pumpServerCopyComplete(p->s_fd, NULL, 0, DISK_OK, p);
	memFree(MEM_4K_BUF, buf);
	return;
    }
    debug(61, 5) ("pumpServerCopy: to FD %d, %d bytes\n", p->s_fd, size);

    commSetTimeout(p->c_fd, Config.Timeout.read, pumpFree, p);
    comm_write(p->s_fd, buf, size, pumpServerCopyComplete, p, memFree4K);
}

static void
pumpServerCopyComplete(int fd, char *bufnotused, size_t size, int errflag, void *data)
{
    PumpStateData *p = data;
    StoreEntry *e = p->buf_entry;
    debug(61, 5) ("pumpServerCopyComplete: called with size=%d (%d,%d)\n", 
		size,p->sent+size, p->cont_len+2);

    if (size <=0 || e->store_status==STORE_ABORTED) {
        debug(61, 5) ("pumpServerCopyComplete: aborted!\n", size);
	pumpFree(fd,data);
	return;
    }
    p->sent += size;
    if (p->cont_len +2 == p->sent || p->cont_len == p->sent) {	
        debug(61, 5) ("pumpServerCopyComplete: Done!\n", size);
	/* we're finished, back off */
	pumpFree(fd, data);
	return;
    }
    debug(61, 5) ("pumpServerCopyComplete: copying from entry at offset %d\n",
	p->sent-size);
    storeClientCopy(e, p->sent, p->sent , 4096,
	memAllocate(MEM_4K_BUF),
	pumpServerCopy, p);
}


static void
pumpReadFromClient(int fd, void *data)
{
    PumpStateData *p = data;
    LOCAL_ARRAY(char, buf, SQUID_TCP_SO_RCVBUF);
    int len=0;

    errno = 0;

    if ((p->rcvd-p->sent) > PUMP_MAXBUFFER) {
        debug(61, 5)("pumpReadFromClient: deferring (%d bytes waiting)\n", 
                p->rcvd-p->sent);
	commSetTimeout(p->c_fd, -1 , NULL, NULL);
    	commSetSelect(p->c_fd, COMM_SELECT_READ, pumpReadFromClient, p, 0);
	commSetDefer(p->c_fd, pumpReadDefer, p);
	return;
    }
	
    len = read(fd, buf, SQUID_TCP_SO_RCVBUF);
    fd_bytes(fd, len, FD_READ);
    commSetDefer(p->c_fd, NULL, NULL);

    debug(61, 5) ("pumpReadFromClient: FD %d: len %d.\n", fd, len);

    if (len > 0)
	commSetTimeout(fd, Config.Timeout.read, pumpFree, p);

    if (len < 0) {
	if (ignoreErrno(errno)) {
    	    debug(61, 5) ("pumpReadFromClient: FD %d: len %d and ignore!\n", fd, len);
	    commSetSelect(fd, COMM_SELECT_READ, pumpReadFromClient, p, 0);
	} else {
	    debug(61,2)("pumpReadFromClient: aborted.\n");
	    storeAbort(p->buf_entry, 0);
	    return;
	}
	debug(61, 2) ("pumpReadFromClient: FD %d: read failure: %s.\n",
	    fd, xstrerror());
    } else if (len == 0 && p->buf_entry->mem_obj->inmem_hi == 0) {
	debug(61, 2) ("pumpReadFromClient: FD %d: failed.\n", fd);
	storeAbort(p->buf_entry, 0);
    } else if (len == 0) {
	/* connection closed, call close handler */
        if (p->rcvd+len==p->cont_len || p->rcvd+len == p->cont_len+2) {
		debug(61, 2) ("pumpReadFromClient: finished!\n");
    		commSetSelect(p->c_fd, COMM_SELECT_READ, NULL, NULL, 0);
		storeComplete(p->buf_entry);
	} else {
		debug(61,4)("pumpReadFromClient: incomplete request or connection broken.\n");
		storeAbort(p->buf_entry, 0);
	}
    } else {
	
	/* ok, go ahead */
	storeAppend(p->buf_entry, buf, len);
	p->rcvd+=len;
	commSetSelect(fd, COMM_SELECT_READ, pumpReadFromClient, p, 10);
    }
}

static int
pumpReadDefer(int fd, void *data)
{
	PumpStateData *p = data;
	debug(61,9)("pumpReadDefer: %d %d\n", p->rcvd,p->sent);
	return (p->rcvd-p->sent) > PUMP_MAXBUFFER;
}


static void
pumpFree(int fd, void *data)
{
    PumpStateData *p=NULL, *q=NULL, *pumpState = (PumpStateData *) data;
    CWCB *hdl;
    void *cbdata;
    int s_fd=0;
    debug(61, 2) ("pumpFree: releasing %p!\n",data);

    for (p=pump_head ; p && p!=pumpState ; q=p, p=p->next);

    if (!p) {
           debug(61,4)("pumpFree: already cleaned.\n");
           return;
    }
    if (q)
           q->next = p->next;
    else
           pump_head = p->next;

    if (p->c_fd>0) {
        debug(61,3)("pumpFree: cleaning client side\n");
    	comm_remove_close_handler(p->c_fd, pumpFree, pumpState);
    	commSetSelect(pumpState->c_fd, COMM_SELECT_READ, NULL, NULL, 0);
    	p->c_fd=-1;
    } else
        debug(61,3)("pumpFree: NOT cleaning server side\n");

    if (p->s_fd>0) {
        debug(61,3)("pumpFree: cleaning server side\n");
    	comm_remove_close_handler(p->s_fd, pumpFree, pumpState);
    	commSetSelect(pumpState->s_fd, COMM_SELECT_READ, NULL, NULL, 0);
 
    	storeUnlockObject(pumpState->buf_entry);
    	storeUnregister(pumpState->buf_entry, pumpState);
    } else
        debug(61,3)("pumpFree: NOT cleaning server side\n");

    requestUnlink(pumpState->req);
    s_fd=pumpState->s_fd;
    pumpState->s_fd = -1;
    hdl = pumpState->callback;
    pumpState->callback=NULL;
    cbdata = pumpState->cbdata;
    cbdataFree(data);
    if (hdl) {
	debug(61,3)("pumpFree: calling handler %p with data %p\n",hdl,cbdata);
    	hdl(s_fd, NULL, 0, 0, cbdata);
    }
    else
        debug(61,3)("pumpFree: no handler to call\n");
}

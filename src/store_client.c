
/*
 * $Id$
 *
 * DEBUG: section 20    Storage Manager Client-Side Interface
 * AUTHOR: Duane Wessels
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by the
 *  National Science Foundation.  Squid is Copyrighted (C) 1998 by
 *  Duane Wessels and the University of California San Diego.  Please
 *  see the COPYRIGHT file for full details.  Squid incorporates
 *  software developed and/or copyrighted by other sources.  Please see
 *  the CREDITS file for full details.
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

#include "squid.h"

/*
 * NOTE: 'Header' refers to the swapfile metadata header.
 *       'Body' refers to the swapfile body, which is the full
 *        HTTP reply (including HTTP headers and body).
 */
static DRCB storeClientReadBody;
static DRCB storeClientReadHeader;
static SIH storeClientFileOpened;
static void storeClientCopy2(StoreEntry * e, store_client * sc);
static void storeClientFileRead(store_client * sc);
static EVH storeClientCopyEvent;
static store_client_t storeClientType(StoreEntry *);

/* check if there is any client waiting for this object at all */
/* return 1 if there is at least one client */
int
storeClientWaiting(const StoreEntry * e)
{
    MemObject *mem = e->mem_obj;
    store_client *sc;
    for (sc = mem->clients; sc; sc = sc->next) {
	if (sc->callback_data != NULL)
	    return 1;
    }
    return 0;
}

store_client *
storeClientListSearch(const MemObject * mem, void *data)
{
    store_client *sc;
    for (sc = mem->clients; sc; sc = sc->next) {
	if (sc->callback_data == data)
	    break;
    }
    return sc;
}

static store_client_t
storeClientType(StoreEntry *e)
{
    MemObject *mem = e->mem_obj;
    /*
     * If some data has been freed from memory, then we must swap in
     */
    if (mem->inmem_lo)
	return STORE_DISK_CLIENT;
    /*
     * If we're not transferring, we have the whole thing and there
     * will be no delays delivering this to the client
     */
    else if (e->store_status != STORE_PENDING)
	return STORE_MEM_CLIENT;
    /*
     * If this is the first client, let it be the mem client
     */
    else if (mem->nclients > 1)
	return STORE_DISK_CLIENT;
    /*
     * otherwise, make subsequent clients read from disk so they
     * can not delay the first, and vice-versa.
     */
    else
	return STORE_MEM_CLIENT;
}

/* add client with fd to client list */
void
storeClientListAdd(StoreEntry * e, void *data)
{
    MemObject *mem = e->mem_obj;
    store_client **T;
    store_client *sc;
    assert(mem);
    if (storeClientListSearch(mem, data) != NULL)
	return;
    mem->nclients++;
    sc = memAllocate(MEM_STORE_CLIENT);
    cbdataAdd(sc, MEM_STORE_CLIENT);	/* sc is callback_data for file_read */
    sc->callback_data = data;
    sc->seen_offset = 0;
    sc->copy_offset = 0;
    sc->swapin_fd = -1;
    sc->flags.disk_io_pending = 0;
    sc->entry = e;
    sc->type = storeClientType(e);
    if (sc->type == STORE_DISK_CLIENT)
	/* assert we'll be able to get the data we want */
	/* maybe we should open swapin_fd here */
	assert(e->swap_file_number > -1);
    for (T = &mem->clients; *T; T = &(*T)->next);
    *T = sc;
}

static void
storeClientCopyEvent(void *data)
{
    store_client *sc = data;
    debug(20, 3) ("storeClientCopyEvent: Running\n");
    sc->flags.copy_event_pending = 0;
    if (!sc->callback)
	return;
    storeClientCopy2(sc->entry, sc);
}

/* copy bytes requested by the client */
void
storeClientCopy(StoreEntry * e,
    off_t seen_offset,
    off_t copy_offset,
    size_t size,
    char *buf,
    STCB * callback,
    void *data)
{
    store_client *sc;
    assert(e->store_status != STORE_ABORTED);
    debug(20, 3) ("storeClientCopy: %s, seen %d, want %d, size %d, cb %p, cbdata %p\n",
	storeKeyText(e->key),
	(int) seen_offset,
	(int) copy_offset,
	(int) size,
	callback,
	data);
    sc = storeClientListSearch(e->mem_obj, data);
    assert(sc != NULL);
    assert(sc->callback == NULL);
    sc->copy_offset = copy_offset;
    sc->seen_offset = seen_offset;
    sc->callback = callback;
    sc->copy_buf = buf;
    sc->copy_size = size;
    sc->copy_offset = copy_offset;
    storeClientCopy2(e, sc);
}

/*
 * This function is used below to decide if we have any more data to
 * send to the client.  If the store_status is STORE_ABORTED, that case
 * should be handled before this function gets called.  If the
 * store_status is STORE_PENDING, then we do have more data to send.
 * If its STORE_OK, then we continue checking.  If the object length is
 * negative, then we don't know the real length and must open the swap
 * file to find out.  If the lenght is >= 0, then we compare it to the
 * requested copy offset.
 */
static int
storeClientNoMoreToSend(StoreEntry * e, store_client * sc)
{
    ssize_t len;
    if (e->store_status != STORE_OK)
	return 0;
    if ((len = objectLen(e)) < 0)
	return 0;
    if (sc->copy_offset < len)
	return 0;
    return 1;
}

static void
storeClientCopy2(StoreEntry * e, store_client * sc)
{
    STCB *callback = sc->callback;
    MemObject *mem = e->mem_obj;
    size_t sz;
    if (sc->flags.copy_event_pending)
	return;
#ifdef PPNR_WIP
    if (EBIT_TEST(e->flags, ENTRY_FWD_HDR_WAIT))
	return;
#endif /* PPNR_WIP */
    if (sc->flags.store_copying) {
	sc->flags.copy_event_pending = 1;
	debug(20, 3) ("storeClientCopy2: Queueing storeClientCopyEvent()\n");
	eventAdd("storeClientCopyEvent", storeClientCopyEvent, sc, 0.0, 0);
	return;
    }
    cbdataLock(sc);		/* ick, prevent sc from getting freed */
    sc->flags.store_copying = 1;
    debug(20, 3) ("storeClientCopy2: %s\n", storeKeyText(e->key));
    assert(callback != NULL);
    if (e->store_status == STORE_ABORTED) {
#if USE_ASYNC_IO
	if (sc->flags.disk_io_pending) {
	    if (sc->swapin_fd >= 0)
		aioCancel(sc->swapin_fd, NULL);
	    else
		aioCancel(-1, sc);
	}
#endif
	sc->flags.disk_io_pending = 0;
	sc->callback = NULL;
	callback(sc->callback_data, sc->copy_buf, 0);
    } else if (storeClientNoMoreToSend(e, sc)) {
	/* There is no more to send! */
#if USE_ASYNC_IO
	if (sc->flags.disk_io_pending) {
	    if (sc->swapin_fd >= 0)
		aioCancel(sc->swapin_fd, NULL);
	    else
		aioCancel(-1, sc);
	}
#endif
	sc->flags.disk_io_pending = 0;
	sc->callback = NULL;
	callback(sc->callback_data, sc->copy_buf, 0);
    } else if (e->store_status == STORE_PENDING && sc->seen_offset >= mem->inmem_hi) {
	/* client has already seen this, wait for more */
	debug(20, 3) ("storeClientCopy2: Waiting for more\n");
    } else if (sc->copy_offset >= mem->inmem_lo && sc->copy_offset < mem->inmem_hi) {
	/* What the client wants is in memory */
	debug(20, 3) ("storeClientCopy2: Copying from memory\n");
	sz = stmemCopy(&mem->data_hdr, sc->copy_offset, sc->copy_buf, sc->copy_size);
#if USE_ASYNC_IO
	if (sc->flags.disk_io_pending) {
	    if (sc->swapin_fd >= 0)
		aioCancel(sc->swapin_fd, NULL);
	    else
		aioCancel(-1, sc);
	}
#endif
	sc->flags.disk_io_pending = 0;
	sc->callback = NULL;
	callback(sc->callback_data, sc->copy_buf, sz);
    } else if (sc->swapin_fd < 0) {
	debug(20, 3) ("storeClientCopy2: Need to open swap in file\n");
	assert(sc->type == STORE_DISK_CLIENT);
	/* gotta open the swapin file */
	/* assert(sc->copy_offset == 0); */
	if (!sc->flags.disk_io_pending) {
	    sc->flags.disk_io_pending = 1;
	    storeSwapInStart(e, storeClientFileOpened, sc);
	} else {
	    debug(20, 2) ("storeClientCopy2: Averted multiple fd operation\n");
	}
    } else {
	debug(20, 3) ("storeClientCopy: reading from disk FD %d\n",
	    sc->swapin_fd);
	assert(sc->type == STORE_DISK_CLIENT);
	if (!sc->flags.disk_io_pending) {
	    storeClientFileRead(sc);
	} else {
	    debug(20, 2) ("storeClientCopy2: Averted multiple fd operation\n");
	}
    }
    sc->flags.store_copying = 0;
    cbdataUnlock(sc);		/* ick, allow sc to be freed */
}

static void
storeClientFileOpened(int fd, void *data)
{
    store_client *sc = data;
    STCB *callback = sc->callback;
    if (fd < 0) {
	debug(20, 3) ("storeClientFileOpened: failed\n");
	sc->flags.disk_io_pending = 0;
	sc->callback = NULL;
	callback(sc->callback_data, sc->copy_buf, -1);
	return;
    }
    sc->swapin_fd = fd;
    storeClientFileRead(sc);
}

static void
storeClientFileRead(store_client * sc)
{
    MemObject *mem = sc->entry->mem_obj;
    assert(sc->callback != NULL);
#ifdef OPTIMISTIC_IO
    sc->flags.disk_io_pending = 1;
#endif
    if (mem->swap_hdr_sz == 0) {
	file_read(sc->swapin_fd,
	    sc->copy_buf,
	    sc->copy_size,
	    0,
	    storeClientReadHeader,
	    sc);
    } else {
	if (sc->entry->swap_status == SWAPOUT_WRITING)
	    assert(mem->swapout.done_offset > sc->copy_offset + mem->swap_hdr_sz);
	file_read(sc->swapin_fd,
	    sc->copy_buf,
	    sc->copy_size,
	    sc->copy_offset + mem->swap_hdr_sz,
	    storeClientReadBody,
	    sc);
    }
#ifndef OPTIMISTIC_IO
    sc->flags.disk_io_pending = 1;
#endif
}

static void
storeClientReadBody(int fd, const char *buf, int len, int flagnotused, void *data)
{
    store_client *sc = data;
    MemObject *mem = sc->entry->mem_obj;
    STCB *callback = sc->callback;
    assert(sc->flags.disk_io_pending);
    sc->flags.disk_io_pending = 0;
    assert(sc->callback != NULL);
    debug(20, 3) ("storeClientReadBody: FD %d, len %d\n", fd, len);
    if (sc->copy_offset == 0 && len > 0 && mem->reply->sline.status == 0)
	httpReplyParse(mem->reply, sc->copy_buf);
    sc->callback = NULL;
    callback(sc->callback_data, sc->copy_buf, len);
}

static void
storeClientReadHeader(int fd, const char *buf, int len, int flagnotused, void *data)
{
    store_client *sc = data;
    StoreEntry *e = sc->entry;
    MemObject *mem = e->mem_obj;
    STCB *callback = sc->callback;
    int swap_hdr_sz = 0;
    size_t body_sz;
    size_t copy_sz;
    tlv *tlv_list;
    assert(sc->flags.disk_io_pending);
    sc->flags.disk_io_pending = 0;
    assert(sc->callback != NULL);
    debug(20, 3) ("storeClientReadHeader: FD %d, len %d\n", fd, len);
    if (len < 0) {
	debug(20, 3) ("storeClientReadHeader: FD %d: %s\n", fd, xstrerror());
	sc->callback = NULL;
	callback(sc->callback_data, sc->copy_buf, len);
	return;
    }
    tlv_list = storeSwapMetaUnpack(buf, &swap_hdr_sz);
    if (tlv_list == NULL) {
	debug(20, 1) ("storeClientReadHeader: failed to unpack meta data\n");
	sc->callback = NULL;
	callback(sc->callback_data, sc->copy_buf, -1);
	return;
    }
    /*
     * XXX Here we should check the meta data and make sure we got
     * the right object.
     */
    storeSwapTLVFree(tlv_list);
    mem->swap_hdr_sz = swap_hdr_sz;
    mem->object_sz = e->swap_file_sz - swap_hdr_sz;
    /*
     * If our last read got some data the client wants, then give
     * it to them, otherwise schedule another read.
     */
    body_sz = len - swap_hdr_sz;
    if (sc->copy_offset < body_sz) {
	/*
	 * we have (part of) what they want
	 */
	copy_sz = XMIN(sc->copy_size, body_sz);
	debug(20, 3) ("storeClientReadHeader: copying %d bytes of body\n",
	    copy_sz);
	xmemmove(sc->copy_buf, sc->copy_buf + swap_hdr_sz, copy_sz);
	if (sc->copy_offset == 0 && len > 0 && mem->reply->sline.status == 0)
	    httpReplyParse(mem->reply, sc->copy_buf);
	sc->callback = NULL;
	callback(sc->callback_data, sc->copy_buf, copy_sz);
	return;
    }
    /*
     * we don't have what the client wants, but at least we now
     * know the swap header size.
     */
    storeClientFileRead(sc);
}

int
storeClientCopyPending(StoreEntry * e, void *data)
{
    /* return 1 if there is a callback registered for this client */
    store_client *sc = storeClientListSearch(e->mem_obj, data);
    if (sc == NULL)
	return 0;
    if (sc->callback == NULL)
	return 0;
    return 1;
}

int
storeUnregister(StoreEntry * e, void *data)
{
    MemObject *mem = e->mem_obj;
    store_client *sc;
    store_client **S;
    STCB *callback;
    if (mem == NULL)
	return 0;
    debug(20, 3) ("storeUnregister: called for '%s'\n", storeKeyText(e->key));
    for (S = &mem->clients; (sc = *S) != NULL; S = &(*S)->next) {
	if (sc->callback_data == data)
	    break;
    }
    if (sc == NULL)
	return 0;
    if (sc == mem->clients) {
	/*
	 * If we are unregistering the _first_ client for this
	 * entry, then we have to reset the client FD to -1.
	 */
	mem->fd = -1;
    }
    *S = sc->next;
    mem->nclients--;
    sc->flags.disk_io_pending = 0;
    if (e->store_status == STORE_OK && e->swap_status != SWAPOUT_DONE)
	storeCheckSwapOut(e);
    if (sc->swapin_fd > -1) {
	commSetSelect(sc->swapin_fd, COMM_SELECT_READ, NULL, NULL, 0);
	file_close(sc->swapin_fd);
	/* XXX this probably leaks file_read handler structures */
    }
#if USE_ASYNC_IO
    else
	aioCancel(-1, sc);
#endif
    if ((callback = sc->callback) != NULL) {
	/* callback with ssize = -1 to indicate unexpected termination */
	debug(20, 3) ("storeUnregister: store_client for %s has a callback\n",
	    mem->url);
	sc->callback = NULL;
	callback(sc->callback_data, sc->copy_buf, -1);
    }
    cbdataFree(sc);
    return 1;
}

off_t
storeLowestMemReaderOffset(const StoreEntry * entry)
{
    const MemObject *mem = entry->mem_obj;
    off_t lowest = mem->inmem_hi;
    store_client *sc;
    store_client *nx = NULL;
    for (sc = mem->clients; sc; sc = nx) {
	nx = sc->next;
	if (sc->callback_data == NULL)	/* open slot */
	    continue;
	if (sc->type != STORE_MEM_CLIENT)
	    continue;
	if (sc->copy_offset < lowest)
	    lowest = sc->copy_offset;
    }
    return lowest;
}

/* Call handlers waiting for  data to be appended to E. */
void
InvokeHandlers(StoreEntry * e)
{
    int i = 0;
    MemObject *mem = e->mem_obj;
    store_client *sc;
    store_client *nx = NULL;
    assert(mem->clients != NULL || mem->nclients == 0);
    debug(20, 3) ("InvokeHandlers: %s\n", storeKeyText(e->key));
    /* walk the entire list looking for valid callbacks */
    for (sc = mem->clients; sc; sc = nx) {
	nx = sc->next;
	debug(20, 3) ("InvokeHandlers: checking client #%d\n", i++);
	if (sc->callback_data == NULL)
	    continue;
	if (sc->callback == NULL)
	    continue;
	storeClientCopy2(e, sc);
    }
}

int
storePendingNClients(const StoreEntry * e)
{
    MemObject *mem = e->mem_obj;
    int npend = NULL == mem ? 0 : mem->nclients;
    debug(20, 3) ("storePendingNClients: returning %d\n", npend);
    return npend;
}


/*
 * $Id$
 *
 * DEBUG: section 20    Storage Manager Swapout Functions
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

typedef struct swapout_ctrl_t {
    char *swapfilename;
    int oldswapstatus;
    StoreEntry *e;
} swapout_ctrl_t;

static FOCB storeSwapOutFileOpened;
static off_t storeSwapOutObjectBytesOnDisk(const MemObject *);
static void storeSwapOutStart(StoreEntry * e);
static DWCB storeSwapOutHandle;

/* start swapping object to disk */
static void
storeSwapOutStart(StoreEntry * e)
{
    swapout_ctrl_t *ctrlp = xmalloc(sizeof(swapout_ctrl_t));
    assert(e->mem_obj);
    cbdataAdd(ctrlp, cbdataXfree, 0);
    storeLockObject(e);
    e->swap_file_number = storeDirMapAllocate();
    ctrlp->swapfilename = xstrdup(storeSwapFullPath(e->swap_file_number, NULL));
    ctrlp->e = e;
    ctrlp->oldswapstatus = e->swap_status;
    e->swap_status = SWAPOUT_OPENING;
    e->mem_obj->swapout.ctrl = ctrlp;
    file_open(ctrlp->swapfilename,
	O_WRONLY | O_CREAT | O_TRUNC,
	storeSwapOutFileOpened,
	ctrlp,
	e);
}

static void
storeSwapOutHandle(int fdnotused, int flag, size_t len, void *data)
{
    swapout_ctrl_t *ctrlp = data;
    StoreEntry *e = ctrlp->e;
    MemObject *mem = e->mem_obj;
    debug(20, 3) ("storeSwapOutHandle: '%s', len=%d\n", storeKeyText(e->key), (int) len);
    if (flag < 0) {
	debug(20, 1) ("storeSwapOutHandle: SwapOut failure (err code = %d).\n",
	    flag);
	e->swap_status = SWAPOUT_NONE;
	if (e->swap_file_number > -1) {
	    storeUnlinkFileno(e->swap_file_number);
	    storeDirMapBitReset(e->swap_file_number);
	    if (flag == DISK_NO_SPACE_LEFT) {
		storeDirDiskFull(e->swap_file_number);
		storeDirConfigure();
		storeConfigure();
	    }
	    e->swap_file_number = -1;
	}
	storeReleaseRequest(e);
	storeSwapOutFileClose(e);
	return;
    }
#if USE_ASYNC_IO
    if (mem == NULL) {
	debug(20, 1) ("storeSwapOutHandle: mem == NULL : Cancelling swapout\n");
	return;
    }
#else
    assert(mem != NULL);
#endif
    assert(mem->swap_hdr_sz != 0);
    mem->swapout.done_offset += len;
    if (e->store_status == STORE_PENDING) {
	storeCheckSwapOut(e);
	return;
    } else if (mem->swapout.done_offset < objectLen(e) + mem->swap_hdr_sz) {
	storeCheckSwapOut(e);
	return;
    }
    /* swapping complete */
    debug(20, 5) ("storeSwapOutHandle: SwapOut complete: '%s' to %s.\n",
	storeUrl(e), storeSwapFullPath(e->swap_file_number, NULL));
    e->swap_file_sz = objectLen(e) + mem->swap_hdr_sz;
    e->swap_status = SWAPOUT_DONE;
    storeDirUpdateSwapSize(e->swap_file_number, e->swap_file_sz, 1);
    if (storeCheckCachable(e)) {
	storeLog(STORE_LOG_SWAPOUT, e);
	storeDirSwapLog(e, SWAP_LOG_ADD);
    }
    /* Note, we don't otherwise call storeReleaseRequest() here because
     * storeCheckCachable() does it for is if necessary */
    storeSwapOutFileClose(e);
}

void
storeCheckSwapOut(StoreEntry * e)
{
    MemObject *mem = e->mem_obj;
    off_t lowest_offset;
    off_t new_mem_lo;
    off_t on_disk;
    size_t swapout_size;
    char *swap_buf;
    ssize_t swap_buf_len;
    int hdr_len = 0;
    if (mem == NULL)
	return;
    /* should we swap something out to disk? */
    debug(20, 7) ("storeCheckSwapOut: %s\n", storeUrl(e));
    debug(20, 7) ("storeCheckSwapOut: store_status = %s\n",
	storeStatusStr[e->store_status]);
    if (EBIT_TEST(e->flags, ENTRY_ABORTED)) {
	assert(EBIT_TEST(e->flags, RELEASE_REQUEST));
	storeSwapOutFileClose(e);
	return;
    }
    debug(20, 7) ("storeCheckSwapOut: mem->inmem_lo = %d\n",
	(int) mem->inmem_lo);
    debug(20, 7) ("storeCheckSwapOut: mem->inmem_hi = %d\n",
	(int) mem->inmem_hi);
    debug(20, 7) ("storeCheckSwapOut: swapout.queue_offset = %d\n",
	(int) mem->swapout.queue_offset);
    debug(20, 7) ("storeCheckSwapOut: swapout.done_offset = %d\n",
	(int) mem->swapout.done_offset);
#if USE_ASYNC_IO
    if (mem->inmem_hi < mem->swapout.queue_offset) {
	storeAbort(e);
	assert(EBIT_TEST(e->flags, RELEASE_REQUEST));
	storeSwapOutFileClose(e);
	return;
    }
#else
    assert(mem->inmem_hi >= mem->swapout.queue_offset);
#endif
    lowest_offset = storeLowestMemReaderOffset(e);
    debug(20, 7) ("storeCheckSwapOut: lowest_offset = %d\n",
	(int) lowest_offset);
    new_mem_lo = lowest_offset;
    assert(new_mem_lo >= mem->inmem_lo);
    /*
     * We should only free up to what we know has been written to
     * disk, not what has been queued for writing.  Otherwise there
     * will be a chunk of the data which is not in memory and is
     * not yet on disk.
     */
    if (storeSwapOutAble(e))
	if ((on_disk = storeSwapOutObjectBytesOnDisk(mem)) < new_mem_lo)
	    new_mem_lo = on_disk;
    stmemFreeDataUpto(&mem->data_hdr, new_mem_lo);
    mem->inmem_lo = new_mem_lo;
    if (e->swap_status == SWAPOUT_WRITING)
	assert(mem->inmem_lo <= mem->swapout.done_offset);
    if (!storeSwapOutAble(e))
	return;
    swapout_size = (size_t) (mem->inmem_hi - mem->swapout.queue_offset);
    debug(20, 7) ("storeCheckSwapOut: swapout_size = %d\n",
	(int) swapout_size);
    if (swapout_size == 0) {
	if (e->store_status == STORE_OK && !storeSwapOutWriteQueued(mem)) {
	    debug(20, 7) ("storeCheckSwapOut: nothing to write for STORE_OK\n");
	    if (e->swap_file_number > -1) {
		storeUnlinkFileno(e->swap_file_number);
		storeDirMapBitReset(e->swap_file_number);
		e->swap_file_number = -1;
	    }
	    e->swap_status = SWAPOUT_NONE;
	    storeReleaseRequest(e);
	    storeSwapOutFileClose(e);
	}
	return;
    }
    if (e->store_status == STORE_PENDING) {
	/* wait for a full block to write */
	if (swapout_size < VM_WINDOW_SZ)
	    return;
	/*
	 * Wait until we are below the disk FD limit, only if the
	 * next server-side read won't be deferred.
	 */
	if (storeTooManyDiskFilesOpen() && !fwdCheckDeferRead(-1, e))
	    return;
    }
    /* Ok, we have stuff to swap out.  Is there a swapout.fd open? */
    if (e->swap_status == SWAPOUT_NONE) {
	assert(mem->swapout.fd == -1);
	assert(mem->inmem_lo == 0);
	if (storeCheckCachable(e))
	    storeSwapOutStart(e);
	/* else ENTRY_CACHABLE will be cleared and we'll never get
	 * here again */
	return;
    }
    if (e->swap_status == SWAPOUT_OPENING)
	return;
    assert(mem->swapout.fd > -1);
    if (swapout_size > STORE_SWAP_BUF)
	swapout_size = STORE_SWAP_BUF;
    swap_buf = memAllocate(MEM_DISK_BUF);
    swap_buf_len = stmemCopy(&mem->data_hdr,
	mem->swapout.queue_offset,
	swap_buf,
	swapout_size);
    if (swap_buf_len < 0) {
	debug(20, 1) ("stmemCopy returned %d for '%s'\n", swap_buf_len, storeKeyText(e->key));
	storeUnlinkFileno(e->swap_file_number);
	storeDirMapBitReset(e->swap_file_number);
	e->swap_file_number = -1;
	e->swap_status = SWAPOUT_NONE;
	memFree(swap_buf, MEM_DISK_BUF);
	storeReleaseRequest(e);
	storeSwapOutFileClose(e);
	return;
    }
    debug(20, 3) ("storeCheckSwapOut: swap_buf_len = %d\n", (int) swap_buf_len);
    assert(swap_buf_len > 0);
    debug(20, 3) ("storeCheckSwapOut: swapping out %d bytes from %d\n",
	swap_buf_len, (int) mem->swapout.queue_offset);
    mem->swapout.queue_offset += swap_buf_len - hdr_len;
    file_write(mem->swapout.fd,
	-1,
	swap_buf,
	swap_buf_len,
	storeSwapOutHandle,
	mem->swapout.ctrl,
	memFreeDISK);
}

void
storeSwapOutFileClose(StoreEntry * e)
{
    MemObject *mem = e->mem_obj;
    swapout_ctrl_t *ctrlp;
    assert(mem != NULL);
    debug(20, 3) ("storeSwapOutFileClose: %s\n", storeKeyText(e->key));
    if (mem->swapout.fd < 0) {
#if USE_ASYNC_IO
	aioCancel(-1, e);	/* Make doubly certain pending ops are gone */
#endif
	return;
    }
    ctrlp = mem->swapout.ctrl;
    file_close(mem->swapout.fd);
    mem->swapout.fd = -1;
    xfree(ctrlp->swapfilename);
    cbdataFree(ctrlp);
    mem->swapout.ctrl = NULL;
    storeUnlockObject(e);
}

static void
storeSwapOutFileOpened(void *data, int fd, int errcode)
{
    swapout_ctrl_t *ctrlp = data;
    StoreEntry *e = ctrlp->e;
    MemObject *mem = e->mem_obj;
    int swap_hdr_sz = 0;
    tlv *tlv_list;
    char *buf;
    if (fd == -2 && errcode == -2) {	/* Cancelled - Clean up */
	xfree(ctrlp->swapfilename);
	cbdataFree(ctrlp);
	mem->swapout.ctrl = NULL;
	return;
    }
    assert(e->swap_status == SWAPOUT_OPENING);
    if (fd < 0) {
	debug(20, 0) ("storeSwapOutFileOpened: Unable to open swapfile: %s\n\t%s\n",
	    ctrlp->swapfilename, xstrerror());
	storeDirMapBitReset(e->swap_file_number);
	e->swap_file_number = -1;
	e->swap_status = ctrlp->oldswapstatus;
	xfree(ctrlp->swapfilename);
	cbdataFree(ctrlp);
	mem->swapout.ctrl = NULL;
	return;
    }
    mem->swapout.fd = (short) fd;
    e->swap_status = SWAPOUT_WRITING;
    debug(20, 5) ("storeSwapOutFileOpened: Begin SwapOut '%s' to FD %d '%s'\n",
	storeUrl(e), fd, ctrlp->swapfilename);
    debug(20, 5) ("swap_file_number=%08X\n", e->swap_file_number);
    tlv_list = storeSwapMetaBuild(e);
    buf = storeSwapMetaPack(tlv_list, &swap_hdr_sz);
    storeSwapTLVFree(tlv_list);
    mem->swap_hdr_sz = (size_t) swap_hdr_sz;
    file_write(mem->swapout.fd,
	-1,
	buf,
	mem->swap_hdr_sz,
	storeSwapOutHandle,
	ctrlp,
	xfree);
}

/*
 * Return 1 if we have some data queued.  If there is no data queued,
 * then 'done_offset' equals 'queued_offset' + 'swap_hdr_sz'
 *
 * done_offset represents data written to disk (including the swap meta
 * header), but queued_offset is relative to the in-memory data, and
 * does not include the meta header.
 */
int
storeSwapOutWriteQueued(MemObject * mem)
{
    /*
     * this function doesn't get called much, so I'm using
     * local variables to improve readability.  pphhbbht.
     */
    off_t queued = mem->swapout.queue_offset;
    off_t done = mem->swapout.done_offset;
    size_t hdr = mem->swap_hdr_sz;
    assert(queued + hdr >= done);
    return (queued + hdr > done);
}


/*
 * How much of the object data is on the disk?
 */
static off_t
storeSwapOutObjectBytesOnDisk(const MemObject * mem)
{
    /*
     * NOTE: done_offset represents the disk file size,
     * not the amount of object data on disk.
     * 
     * If we don't have at least 'swap_hdr_sz' bytes
     * then none of the object data is on disk.
     *
     * This should still be safe if swap_hdr_sz == 0,
     * meaning we haven't even opened the swapout file
     * yet.
     */
    if (mem->swapout.done_offset <= mem->swap_hdr_sz)
	return 0;
    return mem->swapout.done_offset - mem->swap_hdr_sz;
}

/*
 * Is this entry a candidate for writing to disk?
 */
int
storeSwapOutAble(const StoreEntry * e)
{
    store_client *sc;
    if (e->swap_status == SWAPOUT_OPENING)
	return 1;
    if (e->mem_obj->swapout.fd > -1)
	return 1;
    if (e->mem_obj->inmem_lo > 0)
	return 0;
    /* swapout.fd == -1 && inmem_lo == 0 */
    /*
     * If there are DISK clients, we must write to disk
     * even if its not cachable
     */
    for (sc = e->mem_obj->clients; sc; sc = sc->next)
	if (sc->type == STORE_DISK_CLIENT)
	    return 1;
    return EBIT_TEST(e->flags, ENTRY_CACHABLE);
}

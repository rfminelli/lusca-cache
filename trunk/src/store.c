
/*
 * $Id$
 *
 * DEBUG: section 20    Storage Manager
 * AUTHOR: Harvest Derived
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by the
 *  National Science Foundation.  Squid is Copyrighted (C) 1998 by
 *  the Regents of the University of California.  Please see the
 *  COPYRIGHT file for full details.  Squid incorporates software
 *  developed and/or copyrighted by other sources.  Please see the
 *  CREDITS file for full details.
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

#define REBUILD_TIMESTAMP_DELTA_MAX 2

#define STORE_IN_MEM_BUCKETS		(229)

const char *memStatusStr[] =
{
    "NOT_IN_MEMORY",
    "IN_MEMORY"
};

const char *pingStatusStr[] =
{
    "PING_NONE",
    "PING_WAITING",
    "PING_DONE"
};

const char *storeStatusStr[] =
{
    "STORE_OK",
    "STORE_PENDING"
};

const char *swapStatusStr[] =
{
    "SWAPOUT_NONE",
    "SWAPOUT_WRITING",
    "SWAPOUT_DONE"
};

typedef struct lock_ctrl_t {
    SIH *callback;
    void *callback_data;
    StoreEntry *e;
} lock_ctrl_t;

/*
 * local function prototypes
 */
static int storeEntryValidLength(const StoreEntry *);
static void storeGetMemSpace(int);
static void storeHashDelete(StoreEntry *);
static MemObject *new_MemObject(const char *, const char *);
static void destroy_MemObject(StoreEntry *);
static FREE destroy_StoreEntry;
static void storePurgeMem(StoreEntry *);
static int getKeyCounter(void);
static int storeKeepInMemory(const StoreEntry *);
static OBJH storeCheckCachableStats;
static EVH storeLateRelease;

/*
 * local variables
 */
#if HEAP_REPLACEMENT
/*
 * The heap equivalent of inmem_list, inmem_heap, is in globals.c so other
 * modules can access it when updating object metadata (e.g., refcount)
 */
#else
static dlink_list inmem_list;
#endif
int store_pages_max = 0;
int store_swap_high = 0;
int store_swap_low = 0;
static Stack LateReleaseStack;

#if URL_CHECKSUM_DEBUG
unsigned int
url_checksum(const char *url)
{
    unsigned int ck;
    MD5_CTX M;
    static unsigned char digest[16];
    MD5Init(&M);
    MD5Update(&M, (unsigned char *) url, strlen(url));
    MD5Final(digest, &M);
    xmemcpy(&ck, digest, sizeof(ck));
    return ck;
}
#endif

static MemObject *
new_MemObject(const char *url, const char *log_url)
{
    MemObject *mem = memAllocate(MEM_MEMOBJECT);
    mem->reply = httpReplyCreate();
    mem->url = xstrdup(url);
#if URL_CHECKSUM_DEBUG
    mem->chksum = url_checksum(mem->url);
#endif
    mem->log_url = xstrdup(log_url);
    mem->object_sz = -1;
    mem->fd = -1;
    /* XXX account log_url */
    debug(20, 3) ("new_MemObject: returning %p\n", mem);
    return mem;
}

StoreEntry *
new_StoreEntry(int mem_obj_flag, const char *url, const char *log_url)
{
    StoreEntry *e = NULL;
    e = memAllocate(MEM_STOREENTRY);
    if (mem_obj_flag)
	e->mem_obj = new_MemObject(url, log_url);
    debug(20, 3) ("new_StoreEntry: returning %p\n", e);
    e->expires = e->lastmod = e->lastref = e->timestamp = -1;
    e->swap_filen = -1;
    e->swap_dirn = -1;
    return e;
}

static void
destroy_MemObject(StoreEntry * e)
{
    MemObject *mem = e->mem_obj;
    const Ctx ctx = ctx_enter(mem->url);
    debug(20, 3) ("destroy_MemObject: destroying %p\n", mem);
#if URL_CHECKSUM_DEBUG
    assert(mem->chksum == url_checksum(mem->url));
#endif
    e->mem_obj = NULL;
    if (!shutting_down)
	assert(mem->swapout.sio == NULL);
    stmemFree(&mem->data_hdr);
    mem->inmem_hi = 0;
    /*
     * There is no way to abort FD-less clients, so they might
     * still have mem->clients set if mem->fd == -1
     */
    assert(mem->fd == -1 || mem->clients.head == NULL);
    httpReplyDestroy(mem->reply);
    requestUnlink(mem->request);
    mem->request = NULL;
    ctx_exit(ctx);		/* must exit before we free mem->url */
    safe_free(mem->url);
    safe_free(mem->log_url);	/* XXX account log_url */
    memFree(mem, MEM_MEMOBJECT);
}

static void
destroy_StoreEntry(void *data)
{
    StoreEntry *e = data;
    debug(20, 3) ("destroy_StoreEntry: destroying %p\n", e);
    assert(e != NULL);
    if (e->mem_obj)
	destroy_MemObject(e);
    storeHashDelete(e);
    assert(e->key == NULL);
    memFree(e, MEM_STOREENTRY);
}

/* ----- INTERFACE BETWEEN STORAGE MANAGER AND HASH TABLE FUNCTIONS --------- */

void
storeHashInsert(StoreEntry * e, const cache_key * key)
{
    debug(20, 3) ("storeHashInsert: Inserting Entry %p key '%s'\n",
	e, storeKeyText(key));
    e->key = storeKeyDup(key);
    hash_join(store_table, (hash_link *) e);
}

static void
storeHashDelete(StoreEntry * e)
{
    hash_remove_link(store_table, (hash_link *) e);
    storeKeyFree(e->key);
    e->key = NULL;
}

/* -------------------------------------------------------------------------- */


/* get rid of memory copy of the object */
/* Only call this if storeCheckPurgeMem(e) returns 1 */
static void
storePurgeMem(StoreEntry * e)
{
    if (e->mem_obj == NULL)
	return;
    debug(20, 3) ("storePurgeMem: Freeing memory-copy of %s\n",
	storeKeyText(e->key));
    storeSetMemStatus(e, NOT_IN_MEMORY);
    destroy_MemObject(e);
    if (e->swap_status != SWAPOUT_DONE)
	storeRelease(e);
}

void
storeLockObject(StoreEntry * e)
{
    SwapDir *SD;

    if (e->swap_dirn > -1)
	SD = INDEXSD(e->swap_dirn);
    else
	SD = NULL;

    e->lock_count++;
    debug(20, 3) ("storeLockObject: key '%s' count=%d\n",
	storeKeyText(e->key), (int) e->lock_count);
    e->lastref = squid_curtime;
    /* Notify the fs that we're referencing this object again */
    if (SD != NULL && SD->refobj != NULL)
	SD->refobj(SD, e);
}

void
storeReleaseRequest(StoreEntry * e)
{
    if (EBIT_TEST(e->flags, RELEASE_REQUEST))
	return;
    debug(20, 3) ("storeReleaseRequest: '%s'\n", storeKeyText(e->key));
    EBIT_SET(e->flags, RELEASE_REQUEST);
    /*
     * Clear cachable flag here because we might get called before
     * anyone else even looks at the cachability flag.  Also, this
     * prevents httpMakePublic from really setting a public key.
     */
    EBIT_CLR(e->flags, ENTRY_CACHABLE);
    storeSetPrivateKey(e);
}

/* unlock object, return -1 if object get released after unlock
 * otherwise lock_count */
int
storeUnlockObject(StoreEntry * e)
{
    SwapDir *SD;
    e->lock_count--;
    debug(20, 3) ("storeUnlockObject: key '%s' count=%d\n",
	storeKeyText(e->key), e->lock_count);
    if (e->lock_count)
	return (int) e->lock_count;
    if (e->store_status == STORE_PENDING)
	EBIT_SET(e->flags, RELEASE_REQUEST);
    assert(storePendingNClients(e) == 0);
    /* Notify the fs that we're not referencing this object any more */
    if (e->swap_filen > -1)
	SD = INDEXSD(e->swap_dirn);
    else
	SD = NULL;
    if (SD != NULL && SD->unrefobj != NULL)
	SD->unrefobj(SD, e);
#if HEAP_REPLACEMENT
    storeHeapPositionUpdate(e, SD);
#else
#if 0
    /* Note: From 2.4. Not sure how this relates to the unrefobj() call above */
    storeDirLRUDelete(e);
    storeDirLRUAdd(e);
#endif
#endif
    if (EBIT_TEST(e->flags, RELEASE_REQUEST))
	storeRelease(e);
    else if (storeKeepInMemory(e)) {
	storeSetMemStatus(e, IN_MEMORY);
	requestUnlink(e->mem_obj->request);
	e->mem_obj->request = NULL;
    } else {
	storePurgeMem(e);
	if (EBIT_TEST(e->flags, KEY_PRIVATE))
	    debug(20, 1) ("WARNING: %s:%d: found KEY_PRIVATE\n", __FILE__, __LINE__);
    }
    return 0;
}

/* Lookup an object in the cache.
 * return just a reference to object, don't start swapping in yet. */
StoreEntry *
storeGet(const cache_key * key)
{
    debug(20, 3) ("storeGet: looking up %s\n", storeKeyText(key));
    return (StoreEntry *) hash_lookup(store_table, key);
}

StoreEntry *
storeGetPublic(const char *uri, const method_t method)
{
    return storeGet(storeKeyPublic(uri, method));
}

static int
getKeyCounter(void)
{
    static int key_counter = 0;
    if (++key_counter < 0)
	key_counter = 1;
    return key_counter;
}

void
storeSetPrivateKey(StoreEntry * e)
{
    const cache_key *newkey;
    MemObject *mem = e->mem_obj;
    if (e->key && EBIT_TEST(e->flags, KEY_PRIVATE))
	return;			/* is already private */
    if (e->key) {
	if (e->swap_filen > -1)
	    storeDirSwapLog(e, SWAP_LOG_DEL);
	storeHashDelete(e);
    }
    if (mem != NULL) {
	mem->id = getKeyCounter();
	newkey = storeKeyPrivate(mem->url, mem->method, mem->id);
    } else {
	newkey = storeKeyPrivate("JUNK", METHOD_NONE, getKeyCounter());
    }
    assert(hash_lookup(store_table, newkey) == NULL);
    EBIT_SET(e->flags, KEY_PRIVATE);
    storeHashInsert(e, newkey);
}

void
storeSetPublicKey(StoreEntry * e)
{
    StoreEntry *e2 = NULL;
    const cache_key *newkey;
    MemObject *mem = e->mem_obj;
    if (e->key && !EBIT_TEST(e->flags, KEY_PRIVATE))
	return;			/* is already public */
    assert(mem);
    /*
     * We can't make RELEASE_REQUEST objects public.  Depending on
     * when RELEASE_REQUEST gets set, we might not be swapping out
     * the object.  If we're not swapping out, then subsequent
     * store clients won't be able to access object data which has
     * been freed from memory.
     *
     * If RELEASE_REQUEST is set, then ENTRY_CACHABLE should not
     * be set, and storeSetPublicKey() should not be called.
     */
#if HEAP_REPLACEMENT
    if (EBIT_TEST(e->flags, RELEASE_REQUEST))
	debug(20, 1) ("assertion failed: RELEASE key %s, url %s\n",
	    e->key, mem->url);
#endif
    assert(!EBIT_TEST(e->flags, RELEASE_REQUEST));
    newkey = storeKeyPublic(mem->url, mem->method);
    if ((e2 = (StoreEntry *) hash_lookup(store_table, newkey))) {
	debug(20, 3) ("storeSetPublicKey: Making old '%s' private.\n", mem->url);
	storeSetPrivateKey(e2);
	storeRelease(e2);
	newkey = storeKeyPublic(mem->url, mem->method);
    }
    if (e->key)
	storeHashDelete(e);
    EBIT_CLR(e->flags, KEY_PRIVATE);
    storeHashInsert(e, newkey);
    if (e->swap_filen > -1)
	storeDirSwapLog(e, SWAP_LOG_ADD);
}

StoreEntry *
storeCreateEntry(const char *url, const char *log_url, request_flags flags, method_t method)
{
    StoreEntry *e = NULL;
    MemObject *mem = NULL;
    debug(20, 3) ("storeCreateEntry: '%s'\n", url);

    e = new_StoreEntry(STORE_ENTRY_WITH_MEMOBJ, url, log_url);
    e->lock_count = 1;		/* Note lock here w/o calling storeLock() */
    mem = e->mem_obj;
    mem->method = method;
    if (neighbors_do_private_keys || !flags.hierarchical)
	storeSetPrivateKey(e);
    else
	storeSetPublicKey(e);
    if (flags.cachable) {
	EBIT_SET(e->flags, ENTRY_CACHABLE);
	EBIT_CLR(e->flags, RELEASE_REQUEST);
    } else {
	EBIT_CLR(e->flags, ENTRY_CACHABLE);
	storeReleaseRequest(e);
    }
    e->store_status = STORE_PENDING;
    storeSetMemStatus(e, NOT_IN_MEMORY);
    e->swap_status = SWAPOUT_NONE;
    e->swap_filen = -1;
    e->swap_dirn = -1;
    e->refcount = 0;
    e->lastref = squid_curtime;
    e->timestamp = 0;		/* set in storeTimestampsSet() */
    e->ping_status = PING_NONE;
    EBIT_SET(e->flags, ENTRY_VALIDATED);
    return e;
}

/* Mark object as expired */
void
storeExpireNow(StoreEntry * e)
{
    debug(20, 3) ("storeExpireNow: '%s'\n", storeKeyText(e->key));
    e->expires = squid_curtime;
}

/* Append incoming data from a primary server to an entry. */
void
storeAppend(StoreEntry * e, const char *buf, int len)
{
    MemObject *mem = e->mem_obj;
    assert(mem != NULL);
    assert(len >= 0);
    assert(e->store_status == STORE_PENDING);
    if (len) {
	debug(20, 5) ("storeAppend: appending %d bytes for '%s'\n",
	    len,
	    storeKeyText(e->key));
	storeGetMemSpace(len);
	stmemAppend(&mem->data_hdr, buf, len);
	mem->inmem_hi += len;
    }
    if (EBIT_TEST(e->flags, DELAY_SENDING))
	return;
    InvokeHandlers(e);
    storeSwapOut(e);
}

void
#if STDC_HEADERS
storeAppendPrintf(StoreEntry * e, const char *fmt,...)
#else
storeAppendPrintf(va_alist)
     va_dcl
#endif
{
#if STDC_HEADERS
    va_list args;
    va_start(args, fmt);
#else
    va_list args;
    StoreEntry *e = NULL;
    const char *fmt = NULL;
    va_start(args);
    e = va_arg(args, StoreEntry *);
    fmt = va_arg(args, char *);
#endif
    storeAppendVPrintf(e, fmt, args);
    va_end(args);
}

/* used be storeAppendPrintf and Packer */
void
storeAppendVPrintf(StoreEntry * e, const char *fmt, va_list vargs)
{
    LOCAL_ARRAY(char, buf, 4096);
    buf[0] = '\0';
    vsnprintf(buf, 4096, fmt, vargs);
    storeAppend(e, buf, strlen(buf));
}

struct _store_check_cachable_hist {
    struct {
	int non_get;
	int not_entry_cachable;
	int release_request;
	int wrong_content_length;
	int negative_cached;
	int too_big;
	int private_key;
	int too_many_open_files;
	int too_many_open_fds;
    } no;
    struct {
	int Default;
    } yes;
} store_check_cachable_hist;

int
storeTooManyDiskFilesOpen(void)
{
    if (Config.max_open_disk_fds == 0)
	return 0;
    if (store_open_disk_fd > Config.max_open_disk_fds)
	return 1;
    return 0;
}

int
storeCheckCachable(StoreEntry * e)
{
#if CACHE_ALL_METHODS
    if (e->mem_obj->method != METHOD_GET) {
	debug(20, 2) ("storeCheckCachable: NO: non-GET method\n");
	store_check_cachable_hist.no.non_get++;
    } else
#endif
    if (!EBIT_TEST(e->flags, ENTRY_CACHABLE)) {
	debug(20, 2) ("storeCheckCachable: NO: not cachable\n");
	store_check_cachable_hist.no.not_entry_cachable++;
    } else if (EBIT_TEST(e->flags, RELEASE_REQUEST)) {
	debug(20, 2) ("storeCheckCachable: NO: release requested\n");
	store_check_cachable_hist.no.release_request++;
    } else if (e->store_status == STORE_OK && EBIT_TEST(e->flags, ENTRY_BAD_LENGTH)) {
	debug(20, 2) ("storeCheckCachable: NO: wrong content-length\n");
	store_check_cachable_hist.no.wrong_content_length++;
    } else if (EBIT_TEST(e->flags, ENTRY_NEGCACHED)) {
	debug(20, 3) ("storeCheckCachable: NO: negative cached\n");
	store_check_cachable_hist.no.negative_cached++;
	return 0;		/* avoid release call below */
    } else if ((e->mem_obj->reply->content_length > 0 &&
		e->mem_obj->reply->content_length > Config.Store.maxObjectSize) ||
	e->mem_obj->inmem_hi > Config.Store.maxObjectSize) {
	debug(20, 2) ("storeCheckCachable: NO: too big\n");
	store_check_cachable_hist.no.too_big++;
    } else if (e->mem_obj->reply->content_length > (int) Config.Store.maxObjectSize) {
	debug(20, 2) ("storeCheckCachable: NO: too big\n");
	store_check_cachable_hist.no.too_big++;
    } else if (EBIT_TEST(e->flags, KEY_PRIVATE)) {
	debug(20, 3) ("storeCheckCachable: NO: private key\n");
	store_check_cachable_hist.no.private_key++;
    } else if (e->swap_status != SWAPOUT_NONE) {
	/*
	 * here we checked the swap_status because the remaining
	 * cases are only relevant only if we haven't started swapping
	 * out the object yet.
	 */
	return 1;
    } else if (storeTooManyDiskFilesOpen()) {
	debug(20, 2) ("storeCheckCachable: NO: too many disk files open\n");
	store_check_cachable_hist.no.too_many_open_files++;
    } else if (fdNFree() < RESERVED_FD) {
	debug(20, 2) ("storeCheckCachable: NO: too many FD's open\n");
	store_check_cachable_hist.no.too_many_open_fds++;
    } else {
	store_check_cachable_hist.yes.Default++;
	return 1;
    }
    storeReleaseRequest(e);
    EBIT_CLR(e->flags, ENTRY_CACHABLE);
    return 0;
}

static void
storeCheckCachableStats(StoreEntry * sentry)
{
    storeAppendPrintf(sentry, "Category\t Count\n");

    storeAppendPrintf(sentry, "no.non_get\t%d\n",
	store_check_cachable_hist.no.non_get);
    storeAppendPrintf(sentry, "no.not_entry_cachable\t%d\n",
	store_check_cachable_hist.no.not_entry_cachable);
    storeAppendPrintf(sentry, "no.release_request\t%d\n",
	store_check_cachable_hist.no.release_request);
    storeAppendPrintf(sentry, "no.wrong_content_length\t%d\n",
	store_check_cachable_hist.no.wrong_content_length);
    storeAppendPrintf(sentry, "no.negative_cached\t%d\n",
	store_check_cachable_hist.no.negative_cached);
    storeAppendPrintf(sentry, "no.too_big\t%d\n",
	store_check_cachable_hist.no.too_big);
    storeAppendPrintf(sentry, "no.private_key\t%d\n",
	store_check_cachable_hist.no.private_key);
    storeAppendPrintf(sentry, "no.too_many_open_files\t%d\n",
	store_check_cachable_hist.no.too_many_open_files);
    storeAppendPrintf(sentry, "no.too_many_open_fds\t%d\n",
	store_check_cachable_hist.no.too_many_open_fds);
    storeAppendPrintf(sentry, "yes.default\t%d\n",
	store_check_cachable_hist.yes.Default);
}

/* Complete transfer into the local cache.  */
void
storeComplete(StoreEntry * e)
{
    debug(20, 3) ("storeComplete: '%s'\n", storeKeyText(e->key));
    if (e->store_status != STORE_PENDING) {
	/*
	 * if we're not STORE_PENDING, then probably we got aborted
	 * and there should be NO clients on this entry
	 */
	assert(EBIT_TEST(e->flags, ENTRY_ABORTED));
	assert(e->mem_obj->nclients == 0);
	return;
    }
    e->mem_obj->object_sz = e->mem_obj->inmem_hi;
    e->store_status = STORE_OK;
    assert(e->mem_status == NOT_IN_MEMORY);
    if (!storeEntryValidLength(e)) {
	EBIT_SET(e->flags, ENTRY_BAD_LENGTH);
	storeReleaseRequest(e);
    }
#if USE_CACHE_DIGESTS
    if (e->mem_obj->request)
	e->mem_obj->request->hier.store_complete_stop = current_time;
#endif
    InvokeHandlers(e);
    storeSwapOut(e);
}

/*
 * Someone wants to abort this transfer.  Set the reason in the
 * request structure, call the server-side callback and mark the
 * entry for releasing
 */
void
storeAbort(StoreEntry * e)
{
    MemObject *mem = e->mem_obj;
    assert(e->store_status == STORE_PENDING);
    assert(mem != NULL);
    debug(20, 6) ("storeAbort: %s\n", storeKeyText(e->key));
    storeLockObject(e);		/* lock while aborting */
    storeNegativeCache(e);
    storeReleaseRequest(e);
    EBIT_SET(e->flags, ENTRY_ABORTED);
    storeSetMemStatus(e, NOT_IN_MEMORY);
    e->store_status = STORE_OK;
    /*
     * We assign an object length here.  The only other place we assign
     * the object length is in storeComplete()
     */
    mem->object_sz = mem->inmem_hi;
    /* Notify the server side */
    if (mem->abort.callback) {
	eventAdd("mem->abort.callback",
	    mem->abort.callback,
	    mem->abort.data,
	    0.0,
	    0);
	mem->abort.callback = NULL;
	mem->abort.data = NULL;
    }
    /* Notify the client side */
    InvokeHandlers(e);
    /* Close any swapout file */
    storeSwapOutFileClose(e);
    storeUnlockObject(e);	/* unlock */
}

/* Clear Memory storage to accommodate the given object len */
static void
storeGetMemSpace(int size)
{
    StoreEntry *e = NULL;
    int released = 0;
    static time_t last_check = 0;
    int pages_needed;
    int locked = 0;
#if !HEAP_REPLACEMENT
    dlink_node *head;
    dlink_node *m;
    dlink_node *prev = NULL;
#else
    heap_key age;
    heap_key min_age = 0.0;
    link_list *locked_entries = NULL;
#endif
    if (squid_curtime == last_check)
	return;
    last_check = squid_curtime;
    pages_needed = (size / SM_PAGE_SIZE) + 1;
    if (memInUse(MEM_STMEM_BUF) + pages_needed < store_pages_max)
	return;
    debug(20, 2) ("storeGetMemSpace: Starting, need %d pages\n", pages_needed);
#if HEAP_REPLACEMENT
    while (heap_nodes(inmem_heap) > 0) {
	age = heap_peepminkey(inmem_heap);
	e = heap_extractmin(inmem_heap);
	e->mem_obj->node = NULL;	/* no longer in the heap */
	if (storeEntryLocked(e)) {
	    locked++;
	    debug(20, 5) ("storeGetMemSpace: locked key %s\n",
		storeKeyText(e->key));
	    linklistPush(&locked_entries, e);
	    continue;
	}
	released++;
	debug(20, 3) ("Released memory object with key %f size %d refs %d url %s\n",
	    age, e->swap_file_sz, e->refcount, e->mem_obj->url);
	min_age = age;
	storePurgeMem(e);
	if (memInUse(MEM_STMEM_BUF) + pages_needed < store_pages_max)
	    break;
    }
    /*
     * Increase the heap age factor.
     */
    if (min_age > 0)
	inmem_heap->age = min_age;
    /*
     * Reinsert all bumped locked entries back into heap...
     */
    while ((e = linklistShift(&locked_entries)))
	e->mem_obj->node = heap_insert(inmem_heap, e);
#else
    head = inmem_list.head;
    for (m = inmem_list.tail; m; m = prev) {
	if (m == head)
	    break;
	prev = m->prev;
	e = m->data;
	if (storeEntryLocked(e)) {
	    locked++;
	    dlinkDelete(m, &inmem_list);
	    dlinkAdd(e, m, &inmem_list);
	    continue;
	}
	released++;
	storePurgeMem(e);
	if (memInUse(MEM_STMEM_BUF) + pages_needed < store_pages_max)
	    break;
    }
#endif
    debug(20, 3) ("storeGetMemSpace: released %d/%d locked %d\n",
	released, hot_obj_count, locked);
    debug(20, 3) ("storeGetMemSpace stats:\n");
    debug(20, 3) ("  %6d HOT objects\n", hot_obj_count);
    debug(20, 3) ("  %6d were released\n", released);
}

/* The maximum objects to scan for maintain storage space */
#define MAINTAIN_MAX_SCAN	1024
#define MAINTAIN_MAX_REMOVE	64

/*
 * This routine is to be called by main loop in main.c.
 * It removes expired objects on only one bucket for each time called.
 * returns the number of objects removed
 *
 * This should get called 1/s from main().
 */
void
storeMaintainSwapSpace(void *datanotused)
{
    int i;
    SwapDir *SD;

    /* walk each fs */
    for (i = 0; i < Config.cacheSwap.n_configured; i++) {
	/* call the maintain function .. */
	SD = INDEXSD(i);
	if (SD->maintainfs != NULL)
	    SD->maintainfs(SD);
    }

    /* Reregister a maintain event .. */
    eventAdd("MaintainSwapSpace", storeMaintainSwapSpace, NULL, 1.0, 1);
}


/* release an object from a cache */
void
storeRelease(StoreEntry * e)
{
    debug(20, 3) ("storeRelease: Releasing: '%s'\n", storeKeyText(e->key));
    /* If, for any reason we can't discard this object because of an
     * outstanding request, mark it for pending release */
    if (storeEntryLocked(e)) {
	storeExpireNow(e);
	debug(20, 3) ("storeRelease: Only setting RELEASE_REQUEST bit\n");
	storeReleaseRequest(e);
	return;
    }
    if (store_dirs_rebuilding && e->swap_filen > -1) {
	storeSetPrivateKey(e);
	if (e->mem_obj) {
	    storeSetMemStatus(e, NOT_IN_MEMORY);
	    destroy_MemObject(e);
	}
	if (e->swap_filen > -1) {
	    /*
	     * Fake a call to storeLockObject().  When rebuilding is done,
	     * we'll just call storeUnlockObject() on these.
	     */
	    e->lock_count++;
	    EBIT_SET(e->flags, RELEASE_REQUEST);
	    stackPush(&LateReleaseStack, e);
	    return;
	} else {
	    destroy_StoreEntry(e);
	}
    }
    storeLog(STORE_LOG_RELEASE, e);
    if (e->swap_filen > -1) {
	storeUnlink(e);
	if (e->swap_status == SWAPOUT_DONE)
	    if (EBIT_TEST(e->flags, ENTRY_VALIDATED))
		storeDirUpdateSwapSize(&Config.cacheSwap.swapDirs[e->swap_dirn], e->swap_file_sz, -1);
	if (!EBIT_TEST(e->flags, KEY_PRIVATE))
	    storeDirSwapLog(e, SWAP_LOG_DEL);
#if 0
	/* From 2.4. I think we do this in storeUnlink? */
	storeSwapFileNumberSet(e, -1);
#endif
    }
    storeSetMemStatus(e, NOT_IN_MEMORY);
    destroy_StoreEntry(e);
}

static void
storeLateRelease(void *unused)
{
    StoreEntry *e;
    int i;
    static int n = 0;
    if (store_dirs_rebuilding) {
	eventAdd("storeLateRelease", storeLateRelease, NULL, 1.0, 1);
	return;
    }
    for (i = 0; i < 10; i++) {
	e = stackPop(&LateReleaseStack);
	if (e == NULL) {
	    /* done! */
	    debug(20, 1) ("storeLateRelease: released %d objects\n", n);
	    return;
	}
	storeUnlockObject(e);
	n++;
    }
    eventAdd("storeLateRelease", storeLateRelease, NULL, 0.0, 1);
}

/* return 1 if a store entry is locked */
int
storeEntryLocked(const StoreEntry * e)
{
    if (e->lock_count)
	return 1;
    if (e->swap_status == SWAPOUT_WRITING)
	return 1;
    if (e->store_status == STORE_PENDING)
	return 1;
    /*
     * SPECIAL, PUBLIC entries should be "locked"
     */
    if (EBIT_TEST(e->flags, ENTRY_SPECIAL))
	if (!EBIT_TEST(e->flags, KEY_PRIVATE))
	    return 1;
    return 0;
}

static int
storeEntryValidLength(const StoreEntry * e)
{
    int diff;
    const HttpReply *reply;
    assert(e->mem_obj != NULL);
    reply = e->mem_obj->reply;
    debug(20, 3) ("storeEntryValidLength: Checking '%s'\n", storeKeyText(e->key));
    debug(20, 5) ("storeEntryValidLength:     object_len = %d\n",
	objectLen(e));
    debug(20, 5) ("storeEntryValidLength:         hdr_sz = %d\n",
	reply->hdr_sz);
    debug(20, 5) ("storeEntryValidLength: content_length = %d\n",
	reply->content_length);
    if (reply->content_length < 0) {
	debug(20, 5) ("storeEntryValidLength: Unspecified content length: %s\n",
	    storeKeyText(e->key));
	return 1;
    }
    if (reply->hdr_sz == 0) {
	debug(20, 5) ("storeEntryValidLength: Zero header size: %s\n",
	    storeKeyText(e->key));
	return 1;
    }
    if (e->mem_obj->method == METHOD_HEAD) {
	debug(20, 5) ("storeEntryValidLength: HEAD request: %s\n",
	    storeKeyText(e->key));
	return 1;
    }
    if (reply->sline.status == HTTP_NOT_MODIFIED)
	return 1;
    if (reply->sline.status == HTTP_NO_CONTENT)
	return 1;
    diff = reply->hdr_sz + reply->content_length - objectLen(e);
    if (diff == 0)
	return 1;
    debug(20, 3) ("storeEntryValidLength: %d bytes too %s; '%s'\n",
	diff < 0 ? -diff : diff,
	diff < 0 ? "big" : "small",
	storeKeyText(e->key));
    return 0;
}

static void
storeInitHashValues(void)
{
    int i;
    /* Calculate size of hash table (maximum currently 64k buckets).  */
    i = Config.Swap.maxSize / Config.Store.avgObjectSize;
    debug(20, 1) ("Swap maxSize %d KB, estimated %d objects\n",
	Config.Swap.maxSize, i);
    i /= Config.Store.objectsPerBucket;
    debug(20, 1) ("Target number of buckets: %d\n", i);
    /* ideally the full scan period should be configurable, for the
     * moment it remains at approximately 24 hours.  */
    store_hash_buckets = storeKeyHashBuckets(i);
    debug(20, 1) ("Using %d Store buckets\n", store_hash_buckets);
    debug(20, 1) ("Max Mem  size: %d KB\n", Config.memMaxSize >> 10);
    debug(20, 1) ("Max Swap size: %d KB\n", Config.Swap.maxSize);
}

void
storeInit(void)
{
    storeKeyInit();
    storeInitHashValues();
    store_table = hash_create(storeKeyHashCmp,
	store_hash_buckets, storeKeyHashHash);
    storeDigestInit();
    storeLogOpen();
#if HEAP_REPLACEMENT
    inmem_heap = new_heap(1000, HeapKeyGen_StoreEntry_GDSF);
#endif
    stackInit(&LateReleaseStack);
    eventAdd("storeLateRelease", storeLateRelease, NULL, 1.0, 1);
    storeDirInit();
    storeRebuildStart();
    cachemgrRegister("storedir",
	"Store Directory Stats",
	storeDirStats, 0, 1);
    cachemgrRegister("store_check_cachable_stats",
	"storeCheckCachable() Stats",
	storeCheckCachableStats, 0, 1);
}

void
storeConfigure(void)
{
    store_swap_high = (long) (((float) Config.Swap.maxSize *
	    (float) Config.Swap.highWaterMark) / (float) 100);
    store_swap_low = (long) (((float) Config.Swap.maxSize *
	    (float) Config.Swap.lowWaterMark) / (float) 100);
    store_pages_max = Config.memMaxSize / SM_PAGE_SIZE;
}

static int
storeKeepInMemory(const StoreEntry * e)
{
    MemObject *mem = e->mem_obj;
    if (mem == NULL)
	return 0;
    if (mem->data_hdr.head == NULL)
	return 0;
    return mem->inmem_lo == 0;
}

void
storeNegativeCache(StoreEntry * e)
{
    e->expires = squid_curtime + Config.negativeTtl;
    EBIT_SET(e->flags, ENTRY_NEGCACHED);
}

void
storeFreeMemory(void)
{
    hashFreeItems(store_table, destroy_StoreEntry);
    hashFreeMemory(store_table);
    store_table = NULL;
#if USE_CACHE_DIGESTS
    if (store_digest)
	cacheDigestDestroy(store_digest);
#endif
    store_digest = NULL;
}

int
expiresMoreThan(time_t expires, time_t when)
{
    if (expires < 0)		/* No Expires given */
	return 1;
    return (expires > (squid_curtime + when));
}

int
storeEntryValidToSend(StoreEntry * e)
{
    if (EBIT_TEST(e->flags, RELEASE_REQUEST))
	return 0;
    if (EBIT_TEST(e->flags, ENTRY_NEGCACHED))
	if (e->expires <= squid_curtime)
	    return 0;
    if (EBIT_TEST(e->flags, ENTRY_ABORTED))
	return 0;
    return 1;
}

void
storeTimestampsSet(StoreEntry * entry)
{
    const HttpReply *reply = entry->mem_obj->reply;
    time_t served_date = reply->date;
    int age = httpHeaderGetInt(&reply->header, HDR_AGE);
    /*
     * The timestamp calculations below tries to mimic the properties
     * of the age calculation in RFC2616 section 13.2.3. The implementaion
     * isn't complete, and the most notable exception from the RFC is that
     * this does not account for response_delay, but it probably does
     * not matter much as this is calculated immediately when the headers
     * are received, not when the whole response has been received.
     */
    /* make sure that 0 <= served_date <= squid_curtime */
    if (served_date < 0 || served_date > squid_curtime)
	served_date = squid_curtime;
    /*
     * Compensate with Age header if origin server clock is ahead of us
     * and there is a cache in between us and the origin server
     */
    if (age > squid_curtime - served_date)
	served_date = squid_curtime - age;
    entry->expires = reply->expires;
    entry->lastmod = reply->last_modified;
    entry->timestamp = served_date;
}

void
storeRegisterAbort(StoreEntry * e, STABH * cb, void *data)
{
    MemObject *mem = e->mem_obj;
    assert(mem);
    assert(mem->abort.callback == NULL);
    mem->abort.callback = cb;
    mem->abort.data = data;
}

void
storeUnregisterAbort(StoreEntry * e)
{
    MemObject *mem = e->mem_obj;
    assert(mem);
    mem->abort.callback = NULL;
}

void
storeMemObjectDump(MemObject * mem)
{
    debug(20, 1) ("MemObject->data.head: %p\n",
	mem->data_hdr.head);
    debug(20, 1) ("MemObject->data.tail: %p\n",
	mem->data_hdr.tail);
    debug(20, 1) ("MemObject->data.origin_offset: %d\n",
	mem->data_hdr.origin_offset);
    debug(20, 1) ("MemObject->start_ping: %d.%06d\n",
	(int) mem->start_ping.tv_sec,
	(int) mem->start_ping.tv_usec);
    debug(20, 1) ("MemObject->inmem_hi: %d\n",
	(int) mem->inmem_hi);
    debug(20, 1) ("MemObject->inmem_lo: %d\n",
	(int) mem->inmem_lo);
    debug(20, 1) ("MemObject->clients: %p\n",
	mem->clients);
    debug(20, 1) ("MemObject->nclients: %d\n",
	mem->nclients);
    debug(20, 1) ("MemObject->reply: %p\n",
	mem->reply);
    debug(20, 1) ("MemObject->request: %p\n",
	mem->request);
    debug(20, 1) ("MemObject->log_url: %p %s\n",
	mem->log_url,
	checkNullString(mem->log_url));
}

void
storeEntryDump(const StoreEntry * e, int l)
{
    debug(20, l) ("StoreEntry->key: %s\n", storeKeyText(e->key));
    debug(20, l) ("StoreEntry->next: %p\n", e->next);
    debug(20, l) ("StoreEntry->mem_obj: %p\n", e->mem_obj);
    debug(20, l) ("StoreEntry->timestamp: %d\n", (int) e->timestamp);
    debug(20, l) ("StoreEntry->lastref: %d\n", (int) e->lastref);
    debug(20, l) ("StoreEntry->expires: %d\n", (int) e->expires);
    debug(20, l) ("StoreEntry->lastmod: %d\n", (int) e->lastmod);
    debug(20, l) ("StoreEntry->swap_file_sz: %d\n", (int) e->swap_file_sz);
    debug(20, l) ("StoreEntry->refcount: %d\n", e->refcount);
    debug(20, l) ("StoreEntry->flags: %s\n", storeEntryFlags(e));
    debug(20, l) ("StoreEntry->swap_dirn: %d\n", (int) e->swap_dirn);
    debug(20, l) ("StoreEntry->swap_filen: %d\n", (int) e->swap_filen);
    debug(20, l) ("StoreEntry->lock_count: %d\n", (int) e->lock_count);
    debug(20, l) ("StoreEntry->mem_status: %d\n", (int) e->mem_status);
    debug(20, l) ("StoreEntry->ping_status: %d\n", (int) e->ping_status);
    debug(20, l) ("StoreEntry->store_status: %d\n", (int) e->store_status);
    debug(20, l) ("StoreEntry->swap_status: %d\n", (int) e->swap_status);
}

/*
 * NOTE, this function assumes only two mem states
 */
void
storeSetMemStatus(StoreEntry * e, int new_status)
{
    MemObject *mem = e->mem_obj;
    if (new_status == e->mem_status)
	return;
    assert(mem != NULL);
    if (new_status == IN_MEMORY) {
	assert(mem->inmem_lo == 0);
#if HEAP_REPLACEMENT
	if (mem->node == NULL) {
	    if (EBIT_TEST(e->flags, ENTRY_SPECIAL)) {
		debug(20, 4) ("storeSetMemStatus: not inserting special %s\n",
		    mem->url);
	    } else {
		mem->node = heap_insert(inmem_heap, e);
		debug(20, 4) ("storeSetMemStatus: inserted mem node %p\n",
		    mem->node);
	    }
	}
#else
	dlinkAdd(e, &mem->lru, &inmem_list);
#endif
	hot_obj_count++;
    } else {
#if HEAP_REPLACEMENT
	/*
	 * It's being removed from the memory heap; is it already gone?
	 */
	if (mem->node) {
	    heap_delete(inmem_heap, mem->node);
	    debug(20, 4) ("storeSetMemStatus: deleted mem node %p\n",
		mem->node);
	    mem->node = NULL;
	}
#else
	dlinkDelete(&mem->lru, &inmem_list);
#endif
	hot_obj_count--;
    }
    e->mem_status = new_status;
}

const char *
storeUrl(const StoreEntry * e)
{
    if (e == NULL)
	return "[null_entry]";
    else if (e->mem_obj == NULL)
	return "[null_mem_obj]";
    else
	return e->mem_obj->url;
}

void
storeCreateMemObject(StoreEntry * e, const char *url, const char *log_url)
{
    if (e->mem_obj)
	return;
    e->mem_obj = new_MemObject(url, log_url);
}

/* this just sets DELAY_SENDING */
void
storeBuffer(StoreEntry * e)
{
    EBIT_SET(e->flags, DELAY_SENDING);
}

/* this just clears DELAY_SENDING and Invokes the handlers */
void
storeBufferFlush(StoreEntry * e)
{
    EBIT_CLR(e->flags, DELAY_SENDING);
    InvokeHandlers(e);
    storeSwapOut(e);
}

int
objectLen(const StoreEntry * e)
{
    assert(e->mem_obj != NULL);
    return e->mem_obj->object_sz;
}

int
contentLen(const StoreEntry * e)
{
    assert(e->mem_obj != NULL);
    assert(e->mem_obj->reply != NULL);
    return e->mem_obj->object_sz - e->mem_obj->reply->hdr_sz;
}

HttpReply *
storeEntryReply(StoreEntry * e)
{
    if (NULL == e)
	return NULL;
    if (NULL == e->mem_obj)
	return NULL;
    return e->mem_obj->reply;
}

void
storeEntryReset(StoreEntry * e)
{
    MemObject *mem = e->mem_obj;
    debug(20, 3) ("storeEntryReset: %s\n", storeUrl(e));
    assert(mem->swapout.sio == NULL);
    stmemFree(&mem->data_hdr);
    mem->inmem_hi = mem->inmem_lo = 0;
    httpReplyDestroy(mem->reply);
    mem->reply = httpReplyCreate();
    e->expires = e->lastmod = e->timestamp = -1;
}

#if HEAP_REPLACEMENT
/*
 * This routine only handles memory updates these days
 */
void
storeHeapPositionUpdate(StoreEntry * e, SwapDir * SD)
{
    if (e->mem_obj && e->mem_obj->node)
	heap_update(inmem_heap, e->mem_obj->node, e);
}
#endif

/*
 * storeFsInit
 *
 * This routine calls the SETUP routine for each fs type.
 * I don't know where the best place for this is, and I'm not going to shuffle
 * around large chunks of code right now (that can be done once its working.)
 */
void
storeFsInit(void)
{
    storeFsSetup();
}


/*
 * similar to above, but is called when a graceful shutdown is to occur
 * of each fs module.
 */
void
storeFsDone(void)
{
    int i = 0;

    while (storefs_list[i].typestr != NULL) {
	storefs_list[i].donefunc();
	i++;
    }
}

/*
 * called to add another store fs module
 */
void
storeFsAdd(char *type, STSETUP * setup)
{
    int i;
    /* find the number of currently known storefs types */
    for (i = 0; storefs_list && storefs_list[i].typestr; i++) {
	assert(strcmp(storefs_list[i].typestr, type) != 0);
    }
    /* add the new type */
    storefs_list = xrealloc(storefs_list, (i + 2) * sizeof(storefs_entry_t));
    memset(&storefs_list[i + 1], 0, sizeof(storefs_entry_t));
    storefs_list[i].typestr = type;
    /* Call the FS to set up capabilities and initialize the FS driver */
    setup(&storefs_list[i]);
}

#if 0
void
storeSwapFileNumberSet(StoreEntry * e, sfileno filn)
{
    if (e->swap_file_number == filn)
	return;
    if (filn < 0) {
	assert(-1 == filn);
	storeDirMapBitReset(e->swap_file_number);
	storeDirLRUDelete(e);
	e->swap_file_number = -1;
    } else {
	assert(-1 == e->swap_file_number);
	storeDirMapBitSet(e->swap_file_number = filn);
	storeDirLRUAdd(e);
    }
}
#endif

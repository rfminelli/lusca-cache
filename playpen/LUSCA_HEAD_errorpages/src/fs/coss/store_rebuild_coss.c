
/*
 * $Id$
 *
 * DEBUG: section 47    Store COSS Directory Routines
 * AUTHOR: Eric Stern
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

#include "squid.h"

#include "../../libasyncio/aiops.h"
#include "../../libasyncio/async_io.h"
#include "store_coss.h"
#include "store_rebuild_coss.h"

static void storeDirCoss_ReadStripe(RebuildState * rb);

static void
storeCossRebuildComplete(void *data)
{
    RebuildState *rb = data;
    SwapDir *SD = rb->sd;
    CossInfo *cs = SD->fsdata;
    storeCossStartMembuf(SD);
    store_dirs_rebuilding--;
    storeRebuildComplete(&rb->counts);
    debug(47, 1) ("COSS: %s: Rebuild Completed\n", stripePath(SD));
    cs->rebuild.rebuilding = 0;
    debug(47, 1) ("  %d objects scanned, %d objects relocated, %d objects fresher, %d objects ignored\n",
        rb->counts.scancount, rb->cosscounts.reloc, rb->cosscounts.fresher, rb->cosscounts.unknown);
    cbdataFree(rb);
}

static void
storeCoss_AddStoreEntry(RebuildState * rb, const cache_key * key, StoreEntry * e)
{
    StoreEntry *ne;
    SwapDir *SD = rb->sd;
    CossInfo *cs = SD->fsdata;
    rb->counts.objcount++;
    /* The Passed-in store entry is temporary; don't bloody use it directly! */
    assert(e->swap_dirn == SD->index);
    ne = new_StoreEntry(STORE_ENTRY_WITHOUT_MEMOBJ, NULL);
    ne->store_status = STORE_OK;
    storeSetMemStatus(ne, NOT_IN_MEMORY);
    ne->swap_status = SWAPOUT_DONE;
    ne->swap_filen = e->swap_filen;
    ne->swap_dirn = SD->index;
    ne->swap_file_sz = e->swap_file_sz;
    ne->lock_count = 0;
    ne->lastref = e->lastref;
    ne->timestamp = e->timestamp;
    ne->expires = e->expires;
    ne->lastmod = e->lastmod;
    ne->refcount = e->refcount;
    ne->flags = e->flags;
    EBIT_SET(ne->flags, ENTRY_CACHABLE);
    EBIT_CLR(ne->flags, RELEASE_REQUEST);
    EBIT_CLR(ne->flags, KEY_PRIVATE);
    ne->ping_status = PING_NONE;
    EBIT_CLR(ne->flags, ENTRY_VALIDATED);
    storeHashInsert(ne, key);	/* do it after we clear KEY_PRIVATE */
    storeCossAdd(SD, ne, cs->rebuild.curstripe);
    storeEntryDump(ne, 5);
    assert(ne->repl.data != NULL);
    assert(e->repl.data == NULL);
}

static void
storeCoss_DeleteStoreEntry(RebuildState * rb, const cache_key * key, StoreEntry * e)
{
    storeRecycle(e);
}

/*
 * Consider inserting the given StoreEntry into the given
 * COSS directory.
 *
 * The rules for doing this is reasonably simple:
 *
 * If the object doesn't exist in the cache then we simply
 * add it to the current stripe list
 *
 * If the object does exist in the cache then we compare
 * "freshness"; if the newer object is fresher then we
 * remove it from its stripe and re-add it to the current
 * stripe.
 */
static void
storeCoss_ConsiderStoreEntry(RebuildState * rb, const cache_key * key, StoreEntry * e)
{
    StoreEntry *oe;

    /* Check for clashes */
    oe = storeGet(key);
    if (oe == NULL) {
	rb->cosscounts.new++;
	debug(47, 3) ("COSS: Adding filen %d\n", e->swap_filen);
	/* no clash! woo, can add and forget */
	storeCoss_AddStoreEntry(rb, key, e);
	return;
    }
    /* This isn't valid - its possible we have a fresher object in another store */
    /* unlike the UFS-based stores we don't "delete" the disk object when we
     * have deleted the object; its one of the annoying things about COSS. */
    //assert(oe->swap_dirn == SD->index);
    /* Dang, its a clash. See if its fresher */

    /* Fresher? Its a new object: deallocate the old one, reallocate the new one */
    if (e->lastref > oe->lastref) {
	debug(47, 3) ("COSS: fresher object for filen %d found (%ld -> %ld)\n", oe->swap_filen, (long int) oe->timestamp, (long int) e->timestamp);
	rb->cosscounts.fresher++;
	storeCoss_DeleteStoreEntry(rb, key, oe);
	oe = NULL;
	storeCoss_AddStoreEntry(rb, key, e);
	return;
    }
    /*
     * Not fresher? Its the same object then we /should/ probably relocate it; I'm
     * not sure what should be done here.
     */
    if (oe->timestamp == e->timestamp && oe->expires == e->expires) {
	debug(47, 3) ("COSS: filen %d -> %d (since they're the same!)\n", oe->swap_filen, e->swap_filen);
	rb->cosscounts.reloc++;
	storeCoss_DeleteStoreEntry(rb, key, oe);
	oe = NULL;
	storeCoss_AddStoreEntry(rb, key, e);
	return;
    }
    debug(47, 3) ("COSS: filen %d: ignoring this one for some reason\n", e->swap_filen);
    rb->cosscounts.unknown++;
}



/*
 * Take a stripe and attempt to place objects into it
 */
static void
storeDirCoss_ParseStripeBuffer(RebuildState * rb)
{
    SwapDir *SD = rb->sd;
    CossInfo *cs = SD->fsdata;
    tlv *t, *tlv_list;
    int j = 0;
    int bl = 0;
    int tmp;
    squid_off_t *l, len = 0;
    int blocksize = cs->blksz_mask + 1;
    StoreEntry tmpe;
    cache_key key[SQUID_MD5_DIGEST_LENGTH];
    sfileno filen;

    assert(cs->rebuild.rebuilding == 1);
    assert(cs->numstripes > 0);
    assert(cs->rebuild.buf != NULL);

    if (cs->rebuild.buflen == 0) {
	debug(47, 3) ("COSS: %s: stripe %d: read 0 bytes, skipping stripe\n", stripePath(SD), cs->rebuild.curstripe);
	return;
    }
    while (j < cs->rebuild.buflen) {
	l = NULL;
	bl = 0;
	/* XXX there's no bounds checking on the buffer being passed into storeSwapMetaUnpack! */
	tlv_list = storeSwapMetaUnpack(cs->rebuild.buf + j, &bl);
	if (tlv_list == NULL) {
	    debug(47, 3) ("COSS: %s: stripe %d: offset %d gives NULL swapmeta data; end of stripe\n", stripePath(SD), cs->rebuild.curstripe, j);
	    return;
	}
	filen = (off_t) j / (off_t) blocksize + (off_t) ((off_t) cs->rebuild.curstripe * (off_t) COSS_MEMBUF_SZ / (off_t) blocksize);
	debug(47, 3) ("COSS: %s: stripe %d: filen %d: header size %d\n", stripePath(SD), cs->rebuild.curstripe, filen, bl);

	/* COSS objects will have an object size written into the metadata */
	memset(&tmpe, 0, sizeof(tmpe));
	memset(key, 0, sizeof(key));
	for (t = tlv_list; t; t = t->next) {
	    switch (t->type) {
	    case STORE_META_URL:
		debug(47, 3) ("    URL: %s\n", (char *) t->value);
		break;
	    case STORE_META_OBJSIZE:
		l = t->value;
		debug(47, 3) ("Size: %" PRINTF_OFF_T " (len %d)\n", *l, t->length);
		break;
	    case STORE_META_KEY:
		if (t->length != SQUID_MD5_DIGEST_LENGTH) {
		    debug(47, 1) ("COSS: %s: stripe %d: offset %d has invalid STORE_META_KEY length. Ignoring object.\n", stripePath(SD), cs->rebuild.curstripe, j);
		    goto nextobject;
		}
		xmemcpy(key, t->value, SQUID_MD5_DIGEST_LENGTH);
		break;
#if SIZEOF_SQUID_FILE_SZ == SIZEOF_SIZE_T
	    case STORE_META_STD:
		if (t->length != STORE_HDR_METASIZE) {
		    debug(47, 1) ("COSS: %s: stripe %d: offset %d has invalid STORE_META_STD length. Ignoring object.\n", stripePath(SD), cs->rebuild.curstripe, j);
		    goto nextobject;
		}
		xmemcpy(&tmpe.timestamp, t->value, STORE_HDR_METASIZE);
		break;
#else
	    case STORE_META_STD_LFS:
		if (t->length != STORE_HDR_METASIZE) {
		    debug(47, 1) ("COSS: %s: stripe %d: offset %d has invalid STORE_META_STD_LFS length. Ignoring object.\n", stripePath(SD), cs->rebuild.curstripe, j);
		    goto nextobject;
		}
		xmemcpy(&tmpe.timestamp, t->value, STORE_HDR_METASIZE);
		break;
	    case STORE_META_STD:
		if (t->length != STORE_HDR_METASIZE_OLD) {
		    debug(47, 1) ("COSS: %s: stripe %d: offset %d has invalid STORE_META_STD length. Ignoring object.\n", stripePath(SD), cs->rebuild.curstripe, j);
		    goto nextobject;
		} {
		    struct {
			time_t timestamp;
			time_t lastref;
			time_t expires;
			time_t lastmod;
			size_t swap_file_sz;
			u_short refcount;
			u_short flags;
		    }     *tmp = t->value;
		    assert(sizeof(*tmp) == STORE_HDR_METASIZE_OLD);
		    tmpe.timestamp = tmp->timestamp;
		    tmpe.lastref = tmp->lastref;
		    tmpe.expires = tmp->expires;
		    tmpe.lastmod = tmp->lastmod;
		    tmpe.swap_file_sz = tmp->swap_file_sz;
		    tmpe.refcount = tmp->refcount;
		    tmpe.flags = tmp->flags;
		}
		break;
#endif
	    }
	}
	/* Make sure we have an object; if we don't then it may be an indication of trouble */
	if (l == NULL) {
	    debug(47, 3) ("COSS: %s: stripe %d: Object with no size; end of stripe\n", stripePath(SD), cs->rebuild.curstripe);
	    storeSwapTLVFree(tlv_list);
	    return;
	}
	len = *l;
	/* Finally, make sure there's enough data left in this stripe to satisfy the object
	 * we've just been informed about
	 */
	if ((cs->rebuild.buflen - j) < (len + bl)) {
	    debug(47, 3) ("COSS: %s: stripe %d: Not enough data in this stripe for this object, bye bye.\n", stripePath(SD), cs->rebuild.curstripe);
	    storeSwapTLVFree(tlv_list);
	    return;
	}
	/* Houston, we have an object */
	if (storeKeyNull(key)) {
	    debug(47, 3) ("COSS: %s: stripe %d: null data, next!\n", stripePath(SD), cs->rebuild.curstripe);
	    goto nextobject;
	}
	rb->counts.scancount++;
	tmpe.hash.key = key;
	/* Check sizes */
	if (tmpe.swap_file_sz == 0) {
	    tmpe.swap_file_sz = len + bl;
	}
	if (tmpe.swap_file_sz != (len + bl)) {
	    debug(47, 3) ("COSS: %s: stripe %d: file size mismatch (%" PRINTF_OFF_T " != %" PRINTF_OFF_T ")\n", stripePath(SD), cs->rebuild.curstripe, tmpe.swap_file_sz, len);
	    goto nextobject;
	}
	if (EBIT_TEST(tmpe.flags, KEY_PRIVATE)) {
	    debug(47, 3) ("COSS: %s: stripe %d: private key flag set, ignoring.\n", stripePath(SD), cs->rebuild.curstripe);
	    rb->counts.badflags++;
	    goto nextobject;
	}
	/* Time to consider the object! */
	tmpe.swap_filen = filen;
	tmpe.swap_dirn = SD->index;

	debug(47, 3) ("COSS: %s Considering filneumber %d\n", stripePath(SD), tmpe.swap_filen);
	storeCoss_ConsiderStoreEntry(rb, key, &tmpe);

      nextobject:
	/* Free the TLV data */
	storeSwapTLVFree(tlv_list);
	tlv_list = NULL;

	/* Now, advance to the next block-aligned offset after this object */
	j = j + len + bl;
	/* And now, the blocksize! */
	tmp = j / blocksize;
	tmp = (tmp + 1) * blocksize;
	j = tmp;
    }
}

static void
storeDirCoss_ReadStripeComplete(int fd, void *my_data, const char *buf, int aio_return, int aio_errno)
{
    RebuildState *rb = my_data;
    SwapDir *SD = rb->sd;
    CossInfo *cs = SD->fsdata;
    int r_errflag;
    int r_len;
    r_len = aio_return;
    if (aio_errno)
	r_errflag = aio_errno == ENOSPC ? DISK_NO_SPACE_LEFT : DISK_ERROR;
    else
	r_errflag = DISK_OK;
    xmemcpy(cs->rebuild.buf, buf, r_len);

    debug(47, 2) ("COSS: %s: stripe %d, read %d bytes, status %d\n", stripePath(SD), cs->rebuild.curstripe, r_len, r_errflag);
    cs->rebuild.reading = 0;
    if (r_errflag != DISK_OK) {
	debug(47, 2) ("COSS: %s: stripe %d: error! Ignoring objects in this stripe.\n", stripePath(SD), cs->rebuild.curstripe);
	goto nextstripe;
    }
    cs->rebuild.buflen = r_len;
    /* parse the stripe contents */
    /* 
     * XXX note: the read should be put before the parsing so they can happen
     * simultaneously. This'll require some code-shifting so the read buffer
     * and parse buffer are different. This might speed up the read speed;
     * the disk throughput isn't being reached at the present.
     */
    storeDirCoss_ParseStripeBuffer(rb);

  nextstripe:
    cs->rebuild.curstripe++;
    if (cs->rebuild.curstripe >= cs->numstripes) {
	/* Completed the rebuild - move onto the next phase */
	debug(47, 2) ("COSS: %s: completed reading the stripes.\n", stripePath(SD));
	storeCossRebuildComplete(rb);
	return;
    } else {
	/* Next stripe */
	storeDirCoss_ReadStripe(rb);
    }
}

static void
storeDirCoss_ReadStripe(RebuildState * rb)
{
    SwapDir *SD = rb->sd;
    CossInfo *cs = SD->fsdata;

    assert(cs->rebuild.reading == 0);
    cs->rebuild.reading = 1;
    /* Use POSIX AIO for now */
    debug(47, 2) ("COSS: %s: reading stripe %d\n", stripePath(SD), cs->rebuild.curstripe);
    if (cs->rebuild.curstripe > rb->report_current) {
	debug(47, 1) ("COSS: %s: Rebuilding (%d %% completed - %d/%d stripes)\n", stripePath(SD),
	    cs->rebuild.curstripe * 100 / cs->numstripes, cs->rebuild.curstripe, cs->numstripes);
	rb->report_current += rb->report_interval;
    }
    /* XXX this should be a prime candidate to use a modified aioRead which doesn't malloc a damned buffer */
    aioRead(cs->fd, (off_t) cs->rebuild.curstripe * COSS_MEMBUF_SZ, COSS_MEMBUF_SZ, storeDirCoss_ReadStripeComplete, rb);
}

static void
storeDirCoss_StartDiskRebuild(RebuildState * rb)
{
    SwapDir *SD = rb->sd;
    CossInfo *cs = SD->fsdata;
    assert(cs->rebuild.rebuilding == 0);
    assert(cs->numstripes > 0);
    assert(cs->rebuild.buf == NULL);
    assert(cs->fd >= 0);
    cs->rebuild.rebuilding = 1;
    cs->rebuild.curstripe = 0;
    cs->rebuild.buf = xmalloc(COSS_MEMBUF_SZ);
    rb->report_interval = cs->numstripes / COSS_REPORT_INTERVAL;
    rb->report_current = 0;
    debug(47, 2) ("COSS: %s: Beginning disk rebuild.\n", stripePath(SD));
    storeDirCoss_ReadStripe(rb);
}

CBDATA_TYPE(RebuildState);
void
storeCossDirRebuild(SwapDir * sd)
{
    RebuildState *rb;
    int clean = 0;
    CBDATA_INIT_TYPE(RebuildState);
    rb = cbdataAlloc(RebuildState);
    rb->sd = sd;
    rb->flags.clean = (unsigned int) clean;
    debug(20, 1) ("Rebuilding COSS storage in %s (DIRTY)\n", stripePath(sd));
    store_dirs_rebuilding++;
    storeDirCoss_StartDiskRebuild(rb);
}


/*
 * $Id: store_dir_aufs.c 14035 2009-05-03 23:17:56Z adrian.chadd $
 *
 * DEBUG: section 47    Store Directory Routines
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

#include "squid.h"

#include "../../libasyncio/aiops.h"
#include "../../libasyncio/async_io.h"
#include "store_asyncufs.h"
#include "store_bitmap_aufs.h"
#include "store_rebuild_aufs.h"
#include "store_log_aufs.h"

#define STORE_META_BUFSZ 4096

/*
 * The AUFS rebuild process can take one of two main paths - either by logfile
 * or by directory.
 *
 * The logfile rebuild opens the swaplog and a clean swaplog, reads in
 * entries and writes out sanitised entries to the clean swaplog.
 * The clean swaplog is then moved into place over the original swaplog.
 *
 * The directory rebuild opens a "temporary" swaplog and writes out entries
 * to the temporary swaplog as the directory is walked. The temporary
 * swaplog is then moved into place over the original swaplog.
 *
 * Any objects which are to be removed for whatever reason (fresher objects
 * are available, they've expired, etc) are expired via storeRelease().
 * Their deletion will occur once all the stores have rebuilt rather than
 * the deletion taking place during the rebuild.
 */

static void
storeAufsDirRebuildComplete(RebuildState * rb)
{
    if (rb->log_fd) {
	debug(47, 1) ("Done reading %s swaplog (%d entries)\n",
	    rb->sd->path, rb->n_read);
	file_close(rb->log_fd);
	rb->log_fd = -1;
    } else {
	debug(47, 1) ("Done scanning %s (%d entries)\n",
	    rb->sd->path, rb->counts.scancount);
    }
    store_dirs_rebuilding--;
    storeAufsDirCloseTmpSwapLog(rb->sd);
    storeRebuildComplete(&rb->counts);
    if (rb->helper.pid != -1)
	ipcClose(rb->helper.pid, rb->helper.r_fd, rb->helper.w_fd);
    safe_free(rb->rbuf.buf);
    cbdataFree(rb);
}

#if 0
static int
storeAufsDirGetNextFile(RebuildState * rb, sfileno * filn_p, int *size)
{
    SwapDir *SD = rb->sd;
    squidaioinfo_t *aioinfo = (squidaioinfo_t *) SD->fsdata;
    int fd = -1;
    int used = 0;
    int dirs_opened = 0;
    debug(47, 3) ("storeAufsDirGetNextFile: flag=%d, %d: /%02X/%02X\n",
	rb->flags.init,
	rb->sd->index,
	rb->curlvl1,
	rb->curlvl2);
    if (rb->done)
	return -2;
    while (fd < 0 && rb->done == 0) {
	fd = -1;
	if (0 == rb->flags.init) {	/* initialize, open first file */
	    rb->done = 0;
	    rb->curlvl1 = 0;
	    rb->curlvl2 = 0;
	    rb->in_dir = 0;
	    rb->flags.init = 1;
	    assert(Config.cacheSwap.n_configured > 0);
	}
	if (0 == rb->in_dir) {	/* we need to read in a new directory */
	    snprintf(rb->fullpath, SQUID_MAXPATHLEN, "%s/%02X/%02X",
		rb->sd->path,
		rb->curlvl1, rb->curlvl2);
	    if (dirs_opened)
		return -1;
	    rb->td = opendir(rb->fullpath);
	    dirs_opened++;
	    if (rb->td == NULL) {
		debug(47, 1) ("storeAufsDirGetNextFile: opendir: %s: %s\n",
		    rb->fullpath, xstrerror());
	    } else {
		rb->entry = readdir(rb->td);	/* skip . and .. */
		rb->entry = readdir(rb->td);
		if (rb->entry == NULL && errno == ENOENT)
		    debug(47, 1) ("storeAufsDirGetNextFile: directory does not exist!.\n");
		debug(47, 3) ("storeAufsDirGetNextFile: Directory %s\n", rb->fullpath);
	    }
	}
	if (rb->td != NULL && (rb->entry = readdir(rb->td)) != NULL) {
	    rb->in_dir++;
	    if (sscanf(rb->entry->d_name, "%x", &rb->fn) != 1) {
		debug(47, 3) ("storeAufsDirGetNextFile: invalid %s\n",
		    rb->entry->d_name);
		continue;
	    }
	    if (!storeAufsFilenoBelongsHere(rb->fn, rb->sd->index, rb->curlvl1, rb->curlvl2)) {
		debug(47, 3) ("storeAufsDirGetNextFile: %08X does not belong in %d/%d/%d\n",
		    rb->fn, rb->sd->index, rb->curlvl1, rb->curlvl2);
		continue;
	    }
	    used = storeAufsDirMapBitTest(SD, rb->fn);
	    if (used) {
		debug(47, 3) ("storeAufsDirGetNextFile: Locked, continuing with next.\n");
		continue;
	    }
	    snprintf(rb->fullfilename, SQUID_MAXPATHLEN, "%s/%s",
		rb->fullpath, rb->entry->d_name);
	    debug(47, 3) ("storeAufsDirGetNextFile: Opening %s\n", rb->fullfilename);
	    fd = file_open(rb->fullfilename, O_RDONLY | O_BINARY);
	    if (fd < 0)
		debug(47, 1) ("storeAufsDirGetNextFile: %s: %s\n", rb->fullfilename, xstrerror());
	    else
		store_open_disk_fd++;
	    continue;
	}
	if (rb->td != NULL)
	    closedir(rb->td);
	rb->td = NULL;
	rb->in_dir = 0;
	if (++rb->curlvl2 < aioinfo->l2)
	    continue;
	rb->curlvl2 = 0;
	if (++rb->curlvl1 < aioinfo->l1)
	    continue;
	rb->curlvl1 = 0;
	rb->done = 1;
    }
    *filn_p = rb->fn;
    return fd;
}
#endif

/* Add a new object to the cache with empty memory copy and pointer to disk
 * use to rebuild store from disk. */
static StoreEntry *
storeAufsDirAddDiskRestore(SwapDir * SD, const cache_key * key,
    sfileno file_number,
    squid_file_sz swap_file_sz,
    time_t expires,
    time_t timestamp,
    time_t lastref,
    time_t lastmod,
    u_num32 refcount,
    u_short flags,
    int clean)
{
    StoreEntry *e = NULL;
    debug(47, 5) ("storeAufsAddDiskRestore: %s, fileno=%08X\n", storeKeyText(key), file_number);
    /* if you call this you'd better be sure file_number is not 
     * already in use! */
    e = new_StoreEntry(STORE_ENTRY_WITHOUT_MEMOBJ, NULL);
    e->store_status = STORE_OK;
    storeSetMemStatus(e, NOT_IN_MEMORY);
    e->swap_status = SWAPOUT_DONE;
    e->swap_filen = file_number;
    e->swap_dirn = SD->index;
    e->swap_file_sz = swap_file_sz;
    e->lock_count = 0;
    e->lastref = lastref;
    e->timestamp = timestamp;
    e->expires = expires;
    e->lastmod = lastmod;
    e->refcount = refcount;
    e->flags = flags;
    EBIT_SET(e->flags, ENTRY_CACHABLE);
    EBIT_CLR(e->flags, RELEASE_REQUEST);
    EBIT_CLR(e->flags, KEY_PRIVATE);
    e->ping_status = PING_NONE;
    EBIT_CLR(e->flags, ENTRY_VALIDATED);
    storeAufsDirMapBitSet(SD, e->swap_filen);
    storeHashInsert(e, key);	/* do it after we clear KEY_PRIVATE */
    storeAufsDirReplAdd(SD, e);
    return e;
}

#if 0
static void
storeAufsDirRebuildFromDirectory(void *data)
{
    RebuildState *rb = data;
    SwapDir *SD = rb->sd;
    LOCAL_ARRAY(char, hdr_buf, SM_PAGE_SIZE);
    StoreEntry *e = NULL;
    StoreEntry tmpe;
    cache_key key[SQUID_MD5_DIGEST_LENGTH];
    sfileno filn = 0;
    int count;
    int size;
    struct stat sb;
    int swap_hdr_len;
    int fd = -1;
    tlv *tlv_list;
    tlv *t;
    assert(rb != NULL);
    debug(47, 3) ("storeAufsDirRebuildFromDirectory: DIR #%d\n", rb->sd->index);
    for (count = 0; count < rb->speed; count++) {
	assert(fd == -1);
	fd = storeAufsDirGetNextFile(rb, &filn, &size);
	if (fd == -2) {
	    storeAufsDirRebuildComplete(rb);
	    return;
	} else if (fd < 0) {
	    continue;
	}
	assert(fd > -1);
	/* lets get file stats here */
	if (fstat(fd, &sb) < 0) {
	    debug(47, 1) ("storeAufsDirRebuildFromDirectory: fstat(FD %d): %s\n",
		fd, xstrerror());
	    file_close(fd);
	    store_open_disk_fd--;
	    fd = -1;
	    continue;
	}
	if ((++rb->counts.scancount & 0xFFFF) == 0)
	    debug(47, 3) ("  %s %7d files opened so far.\n",
		rb->sd->path, rb->counts.scancount);
	debug(47, 9) ("file_in: fd=%d %08X\n", fd, filn);
	CommStats.syscalls.disk.reads++;
	if (FD_READ_METHOD(fd, hdr_buf, SM_PAGE_SIZE) < 0) {
	    debug(47, 1) ("storeAufsDirRebuildFromDirectory: read(FD %d): %s\n",
		fd, xstrerror());
	    file_close(fd);
	    store_open_disk_fd--;
	    fd = -1;
	    continue;
	}
	file_close(fd);
	store_open_disk_fd--;
	fd = -1;
	swap_hdr_len = 0;
#if USE_TRUNCATE
	if (sb.st_size == 0)
	    continue;
#endif
	tlv_list = storeSwapMetaUnpack(hdr_buf, &swap_hdr_len);
	if (tlv_list == NULL) {
	    debug(47, 1) ("storeAufsDirRebuildFromDirectory: failed to get meta data\n");
	    /* XXX shouldn't this be a call to storeAufsUnlink ? */
	    storeAufsDirUnlinkFile(SD, filn);
	    continue;
	}
	debug(47, 3) ("storeAufsDirRebuildFromDirectory: successful swap meta unpacking\n");
	memset(key, '\0', SQUID_MD5_DIGEST_LENGTH);
	memset(&tmpe, '\0', sizeof(StoreEntry));
	for (t = tlv_list; t; t = t->next) {
	    switch (t->type) {
	    case STORE_META_KEY:
		assert(t->length == SQUID_MD5_DIGEST_LENGTH);
		xmemcpy(key, t->value, SQUID_MD5_DIGEST_LENGTH);
		break;
#if SIZEOF_SQUID_FILE_SZ == SIZEOF_SIZE_T
	    case STORE_META_STD:
		assert(t->length == STORE_HDR_METASIZE);
		xmemcpy(&tmpe.timestamp, t->value, STORE_HDR_METASIZE);
		break;
#else
	    case STORE_META_STD_LFS:
		assert(t->length == STORE_HDR_METASIZE);
		xmemcpy(&tmpe.timestamp, t->value, STORE_HDR_METASIZE);
		break;
	    case STORE_META_STD:
		assert(t->length == STORE_HDR_METASIZE_OLD);
		{
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
	    default:
		break;
	    }
	}
	storeSwapTLVFree(tlv_list);
	tlv_list = NULL;
	if (storeKeyNull(key)) {
	    debug(47, 1) ("storeAufsDirRebuildFromDirectory: NULL key\n");
	    storeAufsDirUnlinkFile(SD, filn);
	    continue;
	}
	tmpe.hash.key = key;
	/* check sizes */
	if (tmpe.swap_file_sz == 0) {
	    tmpe.swap_file_sz = sb.st_size;
	} else if (tmpe.swap_file_sz == sb.st_size - swap_hdr_len) {
	    tmpe.swap_file_sz = sb.st_size;
	} else if (tmpe.swap_file_sz != sb.st_size) {
	    debug(47, 1) ("storeAufsDirRebuildFromDirectory: SIZE MISMATCH %ld!=%ld\n",
		(long int) tmpe.swap_file_sz, (long int) sb.st_size);
	    storeAufsDirUnlinkFile(SD, filn);
	    continue;
	}
	if (EBIT_TEST(tmpe.flags, KEY_PRIVATE)) {
	    storeAufsDirUnlinkFile(SD, filn);
	    rb->counts.badflags++;
	    continue;
	}
	e = storeGet(key);
	if (e && e->lastref >= tmpe.lastref) {
	    /* key already exists, current entry is newer */
	    /* keep old, ignore new */
	    rb->counts.dupcount++;
	    continue;
	} else if (NULL != e) {
	    /* URL already exists, this swapfile not being used */
	    /* junk old, load new */
	    storeRelease(e);	/* release old entry */
	    rb->counts.dupcount++;
	}
	rb->counts.objcount++;
	storeEntryDump(&tmpe, 5);
	e = storeAufsDirAddDiskRestore(SD, key,
	    filn,
	    tmpe.swap_file_sz,
	    tmpe.expires,
	    tmpe.timestamp,
	    tmpe.lastref,
	    tmpe.lastmod,
	    tmpe.refcount,	/* refcount */
	    tmpe.flags,		/* flags */
	    (int) rb->flags.clean);
	storeDirSwapLog(e, SWAP_LOG_ADD);
    }
    eventAdd("storeRebuild", storeAufsDirRebuildFromDirectory, rb, 0.0, 1);
}
#endif

static int
storeAufsDirRebuildFromSwapLogObject(RebuildState *rb, storeSwapLogData s)
{
	SwapDir *SD = rb->sd;
	StoreEntry *e = NULL;
	double x;
	int used;			/* is swapfile already in use? */
	int disk_entry_newer;	/* is the log entry newer than current entry? */

	/*
	 * BC: during 2.4 development, we changed the way swap file
	 * numbers are assigned and stored.  The high 16 bits used
	 * to encode the SD index number.  There used to be a call
	 * to storeDirProperFileno here that re-assigned the index 
	 * bits.  Now, for backwards compatibility, we just need
	 * to mask it off.
	 */
	s.swap_filen &= 0x00FFFFFF;
	debug(47, 3) ("storeAufsDirRebuildFromSwapLog: %s %s %08X\n",
	    swap_log_op_str[(int) s.op],
	    storeKeyText(s.key),
	    s.swap_filen);
	if (s.op == SWAP_LOG_ADD) {
	    (void) 0;
	} else if (s.op == SWAP_LOG_DEL) {
	    /* Delete unless we already have a newer copy */
	    if ((e = storeGet(s.key)) != NULL && s.lastref >= e->lastref) {
		/*
		 * Make sure we don't unlink the file, it might be
		 * in use by a subsequent entry.  Also note that
		 * we don't have to subtract from store_swap_size
		 * because adding to store_swap_size happens in
		 * the cleanup procedure.
		 */
		storeRecycle(e);
		rb->counts.cancelcount++;
	    }
	    return -1;
	} else {
	    x = log(++rb->counts.bad_log_op) / log(10.0);
	    if (0.0 == x - (double) (int) x)
		debug(47, 1) ("WARNING: %d invalid swap log entries found\n",
		    rb->counts.bad_log_op);
	    rb->counts.invalid++;
	    return -1;
	}
	if (!storeAufsDirValidFileno(SD, s.swap_filen, 0)) {
	    rb->counts.invalid++;
	    return -1;
	}
	if (EBIT_TEST(s.flags, KEY_PRIVATE)) {
	    rb->counts.badflags++;
	    return -1;
	}
	e = storeGet(s.key);
	used = storeAufsDirMapBitTest(SD, s.swap_filen);
	/* If this URL already exists in the cache, does the swap log
	 * appear to have a newer entry?  Compare 'lastref' from the
	 * swap log to e->lastref. */
	disk_entry_newer = e ? (s.lastref > e->lastref ? 1 : 0) : 0;
	if (used && !disk_entry_newer) {
	    /* log entry is old, ignore it */
	    rb->counts.clashcount++;
	    return -1;
	} else if (used && e && e->swap_filen == s.swap_filen && e->swap_dirn == SD->index) {
	    /* swapfile taken, same URL, newer, update meta */
	    if (e->store_status == STORE_OK) {
		e->lastref = s.timestamp;
		e->timestamp = s.timestamp;
		e->expires = s.expires;
		e->lastmod = s.lastmod;
		e->flags = s.flags;
		e->refcount += s.refcount;
		storeAufsDirUnrefObj(SD, e);
	    } else {
		debug_trap("storeAufsDirRebuildFromSwapLog: bad condition");
		debug(47, 1) ("\tSee %s:%d\n", __FILE__, __LINE__);
	    }
	    return -1;
	} else if (used) {
	    /* swapfile in use, not by this URL, log entry is newer */
	    /* This is sorta bad: the log entry should NOT be newer at this
	     * point.  If the log is dirty, the filesize check should have
	     * caught this.  If the log is clean, there should never be a
	     * newer entry. */
	    debug(47, 1) ("WARNING: newer swaplog entry for dirno %d, fileno %08X\n",
		SD->index, s.swap_filen);
	    /* I'm tempted to remove the swapfile here just to be safe,
	     * but there is a bad race condition in the NOVM version if
	     * the swapfile has recently been opened for writing, but
	     * not yet opened for reading.  Because we can't map
	     * swapfiles back to StoreEntrys, we don't know the state
	     * of the entry using that file.  */
	    /* We'll assume the existing entry is valid, probably because
	     * the swap file number got taken while we rebuild */
	    rb->counts.clashcount++;
	    return -1;
	} else if (e && !disk_entry_newer) {
	    /* key already exists, current entry is newer */
	    /* keep old, ignore new */
	    rb->counts.dupcount++;
	    return -1;
	} else if (e) {
	    /* key already exists, this swapfile not being used */
	    /* junk old, load new */
	    storeRecycle(e);
	    rb->counts.dupcount++;
	} else {
	    /* URL doesnt exist, swapfile not in use */
	    /* load new */
	    (void) 0;
	}
	/* update store_swap_size */
	rb->counts.objcount++;
	e = storeAufsDirAddDiskRestore(SD, s.key,
	    s.swap_filen,
	    s.swap_file_sz,
	    s.expires,
	    s.timestamp,
	    s.lastref,
	    s.lastmod,
	    s.refcount,
	    s.flags,
	    (int) rb->flags.clean);
	storeDirSwapLog(e, SWAP_LOG_ADD);
	return 1;
}

static void
storeAufsDirRebuildFromSwapLog(void *data)
{
    RebuildState *rb = data;
    char buf[256];
    storeSwapLogData s;
    int count;
    size_t ss;

    assert(rb != NULL);
    if (rb->flags.old_swaplog_entry_size)
	ss = sizeof(storeSwapLogDataOld);
    else
	ss = sizeof(storeSwapLogData);
    assert(ss < sizeof(buf));

    /* load a number of objects per invocation */
    for (count = 0; count < rb->speed; count++) {
	/* Read the swaplog entry, new or old */
	/* XXX this will be slow - one read() per entry .. */
	/* XXX so obviously it needs to be changed and quickly .. */
	if (read(rb->log_fd, buf, ss) != ss) {
	    storeAufsDirRebuildComplete(rb);
	    return;
	}
	rb->n_read++;

	/* Is it an old-style entry? convert it if needed */
	if (rb->flags.old_swaplog_entry_size) {
		(void) storeSwapLogUpgradeEntry(&s, (storeSwapLogDataOld *) buf);
	} else {
		memcpy(&s, buf, sizeof(s));
	}
 	storeAufsDirRebuildFromSwapLogObject(rb, s);

	if ((++rb->counts.scancount & 0xFFF) == 0) {
	    struct stat sb;
	    if (0 == fstat(rb->log_fd, &sb))
		storeRebuildProgress(rb->sd->index, (int) sb.st_size / ss, rb->n_read);
	}
    }
    eventAdd("storeRebuild", storeAufsDirRebuildFromSwapLog, rb, 0.0, 1);
}

#if 0
static void
storeAufsDirRebuildFromSwapLogCheckVersion(void *data)
{
    RebuildState *rb = data;
    storeSwapLogHeader hdr;

    /* XXX should be aioRead() with a callback.. */
    if (read(rb->log_fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
	storeAufsDirRebuildComplete(rb);
	return;
    }
    if (hdr.op == SWAP_LOG_VERSION) {
	if (lseek(rb->log_fd, hdr.record_size, SEEK_SET) < 0) {
	    storeAufsDirRebuildComplete(rb);
	    return;
	}
	if (hdr.version == 1 && hdr.record_size == sizeof(storeSwapLogData)) {
	    rb->flags.old_swaplog_entry_size = 0;
	    eventAdd("storeRebuild", storeAufsDirRebuildFromSwapLog, rb, 0.0, 1);
	    return;
	}
#if SIZEOF_SQUID_FILE_SZ != SIZEOF_SIZE_T
	if (hdr.version == 1 && hdr.record_size == sizeof(storeSwapLogDataOld)) {
	    debug(47, 1) ("storeAufsDirRebuildFromSwapLog: Found current version but without large file support. Upgrading\n");
	    rb->flags.old_swaplog_entry_size = 1;
	    eventAdd("storeRebuild", storeAufsDirRebuildFromSwapLog, rb, 0.0, 1);
	    return;
	}
#endif
	debug(47, 1) ("storeAufsDirRebuildFromSwapLog: Unsupported swap.state version %d size %d\n",
	    hdr.version, hdr.record_size);
	storeAufsDirRebuildComplete(rb);
	return;
    }
    lseek(rb->log_fd, SEEK_SET, 0);
    debug(47, 1) ("storeAufsDirRebuildFromSwapLog: Old version detected. Upgrading\n");
#if SIZEOF_SQUID_FILE_SZ == SIZEOF_SIZE_T
    rb->flags.old_swaplog_entry_size = 0;
    eventAdd("storeRebuild", storeAufsDirRebuildFromSwapLog, rb, 0.0, 1);
#else
    rb->flags.old_swaplog_entry_size = 1;
    eventAdd("storeRebuild", storeAufsDirRebuildFromSwapLog, rb, 0.0, 1);
#endif
}
#endif

static void
storeAufsRebuildHelperRead(int fd, void *data)
{
	RebuildState *rb = data;
	SwapDir *sd = rb->sd;
	/* squidaioinfo_t *aioinfo = (squidaioinfo_t *) sd->fsdata; */
	int r, i;
	storeSwapLogData s;

	assert(fd == rb->helper.r_fd);
	debug(47, 5) ("storeAufsRebuildHelperRead: %s: ready for helper read\n", sd->path);

	assert(rb->rbuf.size - rb->rbuf.used > 0);
	debug(47, 8) ("storeAufsRebuildHelperRead: %s: trying to read %d bytes\n", sd->path, rb->rbuf.size - rb->rbuf.used);
	r = FD_READ_METHOD(fd, rb->rbuf.buf + rb->rbuf.used, rb->rbuf.size - rb->rbuf.used);
	debug(47, 8) ("storeAufsRebuildHelperRead: %s: read %d bytes\n", sd->path, r);
	if (r <= 0) {
		/* Error or EOF */
		debug(47, 1) ("storeAufsRebuildHelperRead: %s: read returned %d; error/eof?\n", sd->path, r);
		ipcClose(rb->helper.pid, rb->helper.r_fd, rb->helper.w_fd);
		rb->helper.pid = rb->helper.r_fd = rb->helper.w_fd = -1;
		storeAufsDirRebuildComplete(rb);
		return;
	}
	rb->rbuf.used += r;

	/* We have some data; process what we can */
	i = 0;
	while (i + sizeof(storeSwapLogData) < rb->rbuf.used) {
		memcpy(&s, rb->rbuf.buf + i, sizeof(storeSwapLogData));
		switch (s.op) {
			case SWAP_LOG_PROGRESS:
				storeRebuildProgress(rb->sd->index,
				    ((storeSwapLogProgress *)(&s))->total, ((storeSwapLogProgress *)(&s))->progress);
				break;
			default:
				rb->n_read++;
				storeAufsDirRebuildFromSwapLogObject(rb, s);
				rb->counts.scancount++;
		}
		i += sizeof(storeSwapLogData);
	}
	debug(47, 5) ("storeAufsRebuildHelperRead: %s: read %d entries\n", sd->path, i / sizeof(storeSwapLogData));

	/* Shuffle what is left to the beginning of the buffer */
	if (i < rb->rbuf.used) {
		memmove(rb->rbuf.buf, rb->rbuf.buf + i, rb->rbuf.used - i);
		rb->rbuf.used -= i;
	}

	/* Re-register */
	commSetSelect(rb->helper.r_fd, COMM_SELECT_READ, storeAufsRebuildHelperRead, rb, 0);
}

CBDATA_TYPE(RebuildState);

/*!
 * @function
 *	storeAufsDirRebuild
 * @abstract
 *	Begin the directory rebuild process for the given AUFS directory
 * @discussion
 *	This function initialises the required bits for the AUFS directory
 *	rebuild, determines whether the rebuild should occur from the
 *	logfile or directory; and begins said process.
 *
 * @param	sd		SwapDir to begin the rebuild process
 */
void
storeAufsDirRebuild(SwapDir * sd)
{
    RebuildState *rb;
    int clean = 0;
    int zero = 0;
    int log_fd;
#if 0
    EVH *func = NULL;
#endif
    CBDATA_INIT_TYPE(RebuildState);
    rb = cbdataAlloc(RebuildState);
    rb->sd = sd;
    const char * args[8];
    char l1[128], l2[128];
    squidaioinfo_t *aioinfo = (squidaioinfo_t *) sd->fsdata;

    /* Open the rebuild helper */
    snprintf(l1, sizeof(l1)-1, "%d", aioinfo->l1);
    snprintf(l2, sizeof(l2)-1, "%d", aioinfo->l2);
    args[0] = "(ufs rebuilding)";
    args[1] = "rebuild";
    args[2] = sd->path;
    args[3] = l1;
    args[4] = l2;
    args[5] = xstrdup(storeAufsDirSwapLogFile(sd, NULL));
    args[6] = NULL;

    rb->helper.pid = ipcCreate(IPC_STREAM, Config.Program.ufs_log_build, args, "ufs rebuilding",
      0, &rb->helper.r_fd, &rb->helper.w_fd, NULL);
    assert(rb->helper.pid != -1);
    safe_free(args[5]);

    /* Setup incoming read buffer */
    /* XXX eww, this should really be in a producer/consumer library damnit */
    rb->rbuf.buf = xmalloc(65536);
    rb->rbuf.size = 65536;
    rb->rbuf.used = 0;

    /* Register for read interest */
    commSetSelect(rb->helper.r_fd, COMM_SELECT_READ, storeAufsRebuildHelperRead, rb, 0);

    /* aaand we begin */
    log_fd = storeAufsDirOpenTmpSwapLog(sd, &clean, &zero);
    file_close(log_fd);	/* We don't need this open anyway..? */

#if 0

    rb->speed = opt_foreground_rebuild ? 1 << 30 : 50;
    /*
     * If the swap.state file exists in the cache_dir, then
     * we'll use storeAufsDirRebuildFromSwapLog(), otherwise we'll
     * use storeAufsDirRebuildFromDirectory() to open up each file
     * and suck in the meta data.
     */
    if (! log_fd || zero) {
	if (log_fd)
	    file_close(log_fd);
	func = storeAufsDirRebuildFromDirectory;
    } else {
	func = storeAufsDirRebuildFromSwapLogCheckVersion;
	rb->log_fd = log_fd;
	rb->flags.clean = (unsigned int) clean;
    }
#endif
    debug(47, 1) ("Rebuilding storage in %s (%s)\n", sd->path, clean ? "CLEAN" : "DIRTY");
    store_dirs_rebuilding++;
#if 0
    eventAdd("storeRebuild", func, rb, 0.0, 1);
#endif
}

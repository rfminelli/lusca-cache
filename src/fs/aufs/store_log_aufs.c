
/*
 * $Id$
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

#define CLEAN_BUF_SZ 16384

struct _clean_state {
    char *cur;
    char *new;
    char *cln;
    char *outbuf;
    int outbuf_offset;
    int fd;
    RemovalPolicyWalker *walker;
};

char *
storeAufsDirSwapLogFile(SwapDir * sd, const char *ext)
{
    LOCAL_ARRAY(char, path, SQUID_MAXPATHLEN);
    LOCAL_ARRAY(char, pathtmp, SQUID_MAXPATHLEN);
    LOCAL_ARRAY(char, digit, 32);
    char *pathtmp2;
    if (Config.Log.swap) {
	xstrncpy(pathtmp, sd->path, SQUID_MAXPATHLEN - 64);
	pathtmp2 = pathtmp;
	while ((pathtmp2 = strchr(pathtmp2, '/')) != NULL)
	    *pathtmp2 = '.';
	while (strlen(pathtmp) && pathtmp[strlen(pathtmp) - 1] == '.')
	    pathtmp[strlen(pathtmp) - 1] = '\0';
	for (pathtmp2 = pathtmp; *pathtmp2 == '.'; pathtmp2++);
	snprintf(path, SQUID_MAXPATHLEN - 64, Config.Log.swap, pathtmp2);
	if (strncmp(path, Config.Log.swap, SQUID_MAXPATHLEN - 64) == 0) {
	    strcat(path, ".");
	    snprintf(digit, 32, "%02d", sd->index);
	    strncat(path, digit, 3);
	}
    } else {
	xstrncpy(path, sd->path, SQUID_MAXPATHLEN - 64);
	strcat(path, "/swap.state");
    }
    if (ext)
	strncat(path, ext, 16);
    return path;
}

void
storeAufsDirOpenSwapLog(SwapDir * sd)
{
    squidaioinfo_t *aioinfo = (squidaioinfo_t *) sd->fsdata;
    char *path;
    int fd;
    path = storeAufsDirSwapLogFile(sd, NULL);
    if (aioinfo->swaplog_fd >= 0) {
	debug(50, 1) ("storeAufsDirOpenSwapLog: %s already open\n", path);
	return;
    }
    fd = file_open(path, O_WRONLY | O_CREAT | O_BINARY);
    if (fd < 0) {
	debug(50, 1) ("%s: %s\n", path, xstrerror());
	fatal("storeAufsDirOpenSwapLog: Failed to open swap log.");
    }
    debug(50, 3) ("Cache Dir #%d log opened on FD %d\n", sd->index, fd);
    aioinfo->swaplog_fd = fd;
}

void
storeAufsDirCloseSwapLog(SwapDir * sd)
{
    squidaioinfo_t *aioinfo = (squidaioinfo_t *) sd->fsdata;
    if (aioinfo->swaplog_fd < 0)	/* not open */
	return;
    file_close(aioinfo->swaplog_fd);
    debug(47, 3) ("Cache Dir #%d log closed on FD %d\n",
	sd->index, aioinfo->swaplog_fd);
    aioinfo->swaplog_fd = -1;
}

void
storeAufsDirCloseTmpSwapLog(SwapDir * sd)
{
    squidaioinfo_t *aioinfo = (squidaioinfo_t *) sd->fsdata;
    char *swaplog_path = xstrdup(storeAufsDirSwapLogFile(sd, NULL));
    char *new_path = xstrdup(storeAufsDirSwapLogFile(sd, ".new"));
    int fd;
    file_close(aioinfo->swaplog_fd);
    if (xrename(new_path, swaplog_path) < 0) {
	fatal("storeAufsDirCloseTmpSwapLog: rename failed");
    }
    fd = file_open(swaplog_path, O_WRONLY | O_CREAT | O_BINARY);
    if (fd < 0) {
	debug(50, 1) ("%s: %s\n", swaplog_path, xstrerror());
	fatal("storeAufsDirCloseTmpSwapLog: Failed to open swap log.");
    }
    safe_free(swaplog_path);
    safe_free(new_path);
    aioinfo->swaplog_fd = fd;
    debug(47, 3) ("Cache Dir #%d log opened on FD %d\n", sd->index, fd);
}

static void
storeSwapLogDataFree(void *s)
{
    memPoolFree(pool_swap_log_data, s);
}

static void
storeAufsWriteSwapLogheader(int fd)
{
    storeSwapLogHeader *hdr = memPoolAlloc(pool_swap_log_data);
    hdr->op = SWAP_LOG_VERSION;
    hdr->version = 1;
    hdr->record_size = sizeof(storeSwapLogData);
    /* The header size is a full log record to keep some level of backward
     * compatibility even if the actual header is smaller
     */
    file_write(fd,
	-1,
	hdr,
	sizeof(storeSwapLogData),
	NULL,
	NULL,
	(FREE *) storeSwapLogDataFree);
}

int
storeAufsDirOpenTmpSwapLog(SwapDir * sd, int *clean_flag, int *zero_flag)
{
    squidaioinfo_t *aioinfo = (squidaioinfo_t *) sd->fsdata;
    char *swaplog_path = xstrdup(storeAufsDirSwapLogFile(sd, NULL));
    char *clean_path = xstrdup(storeAufsDirSwapLogFile(sd, ".last-clean"));
    char *new_path = xstrdup(storeAufsDirSwapLogFile(sd, ".new"));
    struct stat log_sb;
    struct stat clean_sb;
    int fd;

    if (stat(swaplog_path, &log_sb) < 0) {
	debug(47, 1) ("Cache Dir #%d: No log file\n", sd->index);
	safe_free(swaplog_path);
	safe_free(clean_path);
	safe_free(new_path);
	return -1;
    }
    *zero_flag = log_sb.st_size == 0 ? 1 : 0;
    /* close the existing write-only FD */
    if (aioinfo->swaplog_fd >= 0)
	file_close(aioinfo->swaplog_fd);
    /* open a write-only FD for the new log */
    fd = file_open(new_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY);
    if (fd < 0) {
	debug(50, 1) ("%s: %s\n", new_path, xstrerror());
	fatal("storeDirOpenTmpSwapLog: Failed to open swap log.");
    }
    aioinfo->swaplog_fd = fd;
    storeAufsWriteSwapLogheader(fd);
    /* open a read-only stream of the old log */

    fd = file_open(swaplog_path, O_RDONLY | O_BINARY);
    if (fd < 0) {
	debug(50, 0) ("%s: %s\n", swaplog_path, xstrerror());
	fatal("Failed to open swap log for reading");
    }
    memset(&clean_sb, '\0', sizeof(struct stat));
    if (stat(clean_path, &clean_sb) < 0)
	*clean_flag = 0;
    else if (clean_sb.st_mtime < log_sb.st_mtime)
	*clean_flag = 0;
    else
	*clean_flag = 1;
    safeunlink(clean_path, 1);
    safe_free(swaplog_path);
    safe_free(clean_path);
    safe_free(new_path);
    return fd;
}

/*
 * Get the next entry that is a candidate for clean log writing
 */
const StoreEntry *
storeAufsDirCleanLogNextEntry(SwapDir * sd)
{
    const StoreEntry *entry = NULL;
    struct _clean_state *state = sd->log.clean.state;
    if (state->walker)
	entry = state->walker->Next(state->walker);
    return entry;
}

/*
 * "write" an entry to the clean log file.
 */
static void
storeAufsDirWriteCleanEntry(SwapDir * sd, const StoreEntry * e)
{
    storeSwapLogData s;
    static size_t ss = sizeof(storeSwapLogData);
    struct _clean_state *state = sd->log.clean.state;
    memset(&s, '\0', ss);
    s.op = (char) SWAP_LOG_ADD;
    s.swap_filen = e->swap_filen;
    s.timestamp = e->timestamp;
    s.lastref = e->lastref;
    s.expires = e->expires;
    s.lastmod = e->lastmod;
    s.swap_file_sz = e->swap_file_sz;
    s.refcount = e->refcount;
    s.flags = e->flags;
    xmemcpy(&s.key, e->hash.key, SQUID_MD5_DIGEST_LENGTH);
    xmemcpy(state->outbuf + state->outbuf_offset, &s, ss);
    state->outbuf_offset += ss;
    /* buffered write */
    if (state->outbuf_offset + ss > CLEAN_BUF_SZ) {
	if (FD_WRITE_METHOD(state->fd, state->outbuf, state->outbuf_offset) < 0) {
	    debug(50, 0) ("storeDirWriteCleanLogs: %s: write: %s\n",
		state->new, xstrerror());
	    debug(50, 0) ("storeDirWriteCleanLogs: Current swap logfile not replaced.\n");
	    file_close(state->fd);
	    state->fd = -1;
	    unlink(state->new);
	    safe_free(state);
	    sd->log.clean.state = NULL;
	    sd->log.clean.write = NULL;
	    return;
	}
	state->outbuf_offset = 0;
    }
}

/*!
 * @function
 *	storeAufsDirWriteCleanDone
 * @abstract
 *	Complete the "clean" log file process.
 * @discussion
 *	This function finalises the clean log file service done at
 *	shutdown and rotation. The final buffered objects are
 *	written; the file is closed; the store walker is finalised
 *	and various buffers are freed, including the "clean_state".
 *
 * @param	sd	SwapDir
 */
void
storeAufsDirWriteCleanDone(SwapDir * sd)
{
    int fd;
    struct _clean_state *state = sd->log.clean.state;
    if (NULL == state)
	return;
    if (state->fd < 0)
	return;
    state->walker->Done(state->walker);
    if (FD_WRITE_METHOD(state->fd, state->outbuf, state->outbuf_offset) < 0) {
	debug(50, 0) ("storeDirWriteCleanLogs: %s: write: %s\n",
	    state->new, xstrerror());
	debug(50, 0) ("storeDirWriteCleanLogs: Current swap logfile "
	    "not replaced.\n");
	file_close(state->fd);
	state->fd = -1;
	unlink(state->new);
    }
    safe_free(state->outbuf);
    /*
     * You can't rename open files on Microsoft "operating systems"
     * so we have to close before renaming.
     */
    storeAufsDirCloseSwapLog(sd);
    /* save the fd value for a later test */
    fd = state->fd;
    /* rename */
    if (state->fd >= 0) {
#if defined(_SQUID_OS2_) || defined(_SQUID_WIN32_)
	file_close(state->fd);
	state->fd = -1;
#endif
	xrename(state->new, state->cur);
    }
    /* touch a timestamp file if we're not still validating */
    if (store_dirs_rebuilding)
	(void) 0;
    else if (fd < 0)
	(void) 0;
    else
	file_close(file_open(state->cln, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY));
    /* close */
    safe_free(state->cur);
    safe_free(state->new);
    safe_free(state->cln);
    if (state->fd >= 0)
	file_close(state->fd);
    state->fd = -1;
    safe_free(state);
    sd->log.clean.state = NULL;
    sd->log.clean.write = NULL;
}

/*!
 * @function
 *	storeAufsDirSwapLog
 * @abstract
 *	Write the given store entry + operation to the currently open swaplog
 * @discussion
 *	The file_write() sync disk API is used so write combining is done;
 *	however the disk IO happens synchronously.
 *
 * @param	sd	SwapDir to write the log entry for
 * @param	e	StoreEntry to write the swaplog entry for
 * @param	op	Operation - ADD or DEL
 */
void
storeAufsDirSwapLog(const SwapDir * sd, const StoreEntry * e, int op)
{
    squidaioinfo_t *aioinfo = (squidaioinfo_t *) sd->fsdata;
    storeSwapLogData *s = memPoolAlloc(pool_swap_log_data);
    s->op = (char) op;
    s->swap_filen = e->swap_filen;
    s->timestamp = e->timestamp;
    s->lastref = e->lastref;
    s->expires = e->expires;
    s->lastmod = e->lastmod;
    s->swap_file_sz = e->swap_file_sz;
    s->refcount = e->refcount;
    s->flags = e->flags;
    xmemcpy(s->key, e->hash.key, SQUID_MD5_DIGEST_LENGTH);
    file_write(aioinfo->swaplog_fd,
	-1,
	s,
	sizeof(storeSwapLogData),
	NULL,
	NULL,
	(FREE *) storeSwapLogDataFree);
}

/*
 * Begin the process to write clean cache state.  For AUFS this means
 * opening some log files and allocating write buffers.  Return 0 if
 * we succeed, and assign the 'func' and 'data' return pointers.
 */
int
storeAufsDirWriteCleanStart(SwapDir * sd)
{
    struct _clean_state *state = xcalloc(1, sizeof(*state));
#if HAVE_FCHMOD
    struct stat sb;
#endif
    sd->log.clean.write = NULL;
    sd->log.clean.state = NULL;
    state->new = xstrdup(storeAufsDirSwapLogFile(sd, ".clean"));
    state->fd = file_open(state->new, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY);
    if (state->fd < 0) {
	debug(50, 0) ("storeDirWriteCleanStart: %s: open: %s\n",
	    state->new, xstrerror());
	debug(50, 0) ("storeDirWriteCleanStart: Current swap logfile "
	    "not replaced.\n");
	xfree(state->new);
	xfree(state);
	return -1;
    }
    storeAufsWriteSwapLogheader(state->fd);
    state->cur = xstrdup(storeAufsDirSwapLogFile(sd, NULL));
    state->cln = xstrdup(storeAufsDirSwapLogFile(sd, ".last-clean"));
    state->outbuf = xcalloc(CLEAN_BUF_SZ, 1);
    state->outbuf_offset = 0;
    state->walker = sd->repl->WalkInit(sd->repl);
    unlink(state->cln);
    debug(47, 3) ("storeDirWriteCleanLogs: opened %s, FD %d\n",
	state->new, state->fd);
#if HAVE_FCHMOD
    if (stat(state->cur, &sb) == 0)
	fchmod(state->fd, sb.st_mode);
#endif
    sd->log.clean.write = storeAufsDirWriteCleanEntry;
    sd->log.clean.state = state;
    return 0;
}


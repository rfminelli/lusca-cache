/*
 * $Id$
 *
 * DEBUG: section 6     Disk I/O Routines
 * AUTHOR: Harvest Derived
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

/*
 * Copyright (c) 1994, 1995.  All rights reserved.
 *  
 *   The Harvest software was developed by the Internet Research Task
 *   Force Research Group on Resource Discovery (IRTF-RD):
 *  
 *         Mic Bowman of Transarc Corporation.
 *         Peter Danzig of the University of Southern California.
 *         Darren R. Hardy of the University of Colorado at Boulder.
 *         Udi Manber of the University of Arizona.
 *         Michael F. Schwartz of the University of Colorado at Boulder.
 *         Duane Wessels of the University of Colorado at Boulder.
 *  
 *   This copyright notice applies to software in the Harvest
 *   ``src/'' directory only.  Users should consult the individual
 *   copyright notices in the ``components/'' subdirectories for
 *   copyright information about other software bundled with the
 *   Harvest source code distribution.
 *  
 * TERMS OF USE
 *   
 *   The Harvest software may be used and re-distributed without
 *   charge, provided that the software origin and research team are
 *   cited in any use of the system.  Most commonly this is
 *   accomplished by including a link to the Harvest Home Page
 *   (http://harvest.cs.colorado.edu/) from the query page of any
 *   Broker you deploy, as well as in the query result pages.  These
 *   links are generated automatically by the standard Broker
 *   software distribution.
 *   
 *   The Harvest software is provided ``as is'', without express or
 *   implied warranty, and with no support nor obligation to assist
 *   in its use, correction, modification or enhancement.  We assume
 *   no liability with respect to the infringement of copyrights,
 *   trade secrets, or any patents, and are not responsible for
 *   consequential damages.  Proper use of the Harvest software is
 *   entirely the responsibility of the user.
 *  
 * DERIVATIVE WORKS
 *  
 *   Users may make derivative works from the Harvest software, subject 
 *   to the following constraints:
 *  
 *     - You must include the above copyright notice and these 
 *       accompanying paragraphs in all forms of derivative works, 
 *       and any documentation and other materials related to such 
 *       distribution and use acknowledge that the software was 
 *       developed at the above institutions.
 *  
 *     - You must notify IRTF-RD regarding your distribution of 
 *       the derivative work.
 *  
 *     - You must clearly notify users that your are distributing 
 *       a modified version and not the original Harvest software.
 *  
 *     - Any derivative product is also subject to these copyright 
 *       and use restrictions.
 *  
 *   Note that the Harvest software is NOT in the public domain.  We
 *   retain copyright, as specified above.
 *  
 * HISTORY OF FREE SOFTWARE STATUS
 *  
 *   Originally we required sites to license the software in cases
 *   where they were going to build commercial products/services
 *   around Harvest.  In June 1995 we changed this policy.  We now
 *   allow people to use the core Harvest software (the code found in
 *   the Harvest ``src/'' directory) for free.  We made this change
 *   in the interest of encouraging the widest possible deployment of
 *   the technology.  The Harvest software is really a reference
 *   implementation of a set of protocols and formats, some of which
 *   we intend to standardize.  We encourage commercial
 *   re-implementations of code complying to this set of standards.  
 */

#include "squid.h"

#define DISK_LINE_LEN  1024

typedef struct disk_ctrl_t {
    int fd;
    void *data;
} disk_ctrl_t;


typedef struct open_ctrl_t {
    void (*callback) ();
    void *callback_data;
    char *path;
} open_ctrl_t;


typedef struct _dwalk_ctrl {
    int fd;
    off_t offset;
    char *buf;			/* line buffer */
    int cur_len;		/* line len */
    FILE_WALK_HD *handler;
    void *client_data;
    FILE_WALK_LHD *line_handler;
    void *line_data;
} dwalk_ctrl;

static int diskHandleWriteComplete _PARAMS((void *, int, int));
static int diskHandleReadComplete _PARAMS((void *, int, int));
static int diskHandleWalkComplete _PARAMS((void *, int, int));
static PF diskHandleWalk;
static PF diskHandleRead;
static PF diskHandleWrite;
static void file_open_complete _PARAMS((void *, int, int));

/* initialize table */
int
disk_init(void)
{
    return 0;
}

/* Open a disk file. Return a file descriptor */
int
file_open(const char *path, int (*handler) _PARAMS((void)), int mode, void (*callback) (), void *callback_data)
{
    int fd;
    open_ctrl_t *ctrlp;

    ctrlp = xmalloc(sizeof(open_ctrl_t));
    ctrlp->path = xstrdup(path);
    ctrlp->callback = callback;
    ctrlp->callback_data = callback_data;

    if (mode & O_WRONLY)
	mode |= O_APPEND;
    mode |= SQUID_NONBLOCK;

    /* Open file */
#if USE_ASYNC_IO
    if (callback == NULL) {
	fd = open(path, mode, 0644);
	file_open_complete(ctrlp, fd, errno);
	if (fd < 0)
	    return DISK_ERROR;
	return fd;
    }
    aioOpen(path, mode, 0644, file_open_complete, ctrlp);
    return DISK_OK;
#else
    fd = open(path, mode, 0644);
    file_open_complete(ctrlp, fd, errno);
    if (fd < 0)
	return DISK_ERROR;
    return fd;
#endif
}


static void
file_open_complete(void *data, int fd, int errcode)
{
    open_ctrl_t *ctrlp = (open_ctrl_t *) data;
    FD_ENTRY *fde;
    if (fd < 0) {
	errno = errcode;
	debug(50, 0, "file_open: error opening file %s: %s\n", ctrlp->path,
	    xstrerror());
	if (ctrlp->callback)
	    (ctrlp->callback) (ctrlp->callback_data, DISK_ERROR);
	xfree(ctrlp->path);
	xfree(ctrlp);
	return;
    }
    commSetCloseOnExec(fd);
    fd_open(fd, FD_FILE, ctrlp->path);
    fde = &fd_table[fd];
    if (ctrlp->callback)
	(ctrlp->callback) (ctrlp->callback_data, fd);
    xfree(ctrlp->path);
    xfree(ctrlp);
}

/* must close a disk file */

void
file_must_close(int fd)
{
    dwrite_q *q = NULL;
    FD_ENTRY *fde = &fd_table[fd];
    if (fde->type != FD_FILE)
	fatal_dump("file_must_close: type != FD_FILE");
    if (!fde->open)
	fatal_dump("file_must_close: FD not opened");
    BIT_SET(fde->flags, FD_CLOSE_REQUEST);
    BIT_RESET(fde->flags, FD_WRITE_DAEMON);
    BIT_RESET(fde->flags, FD_WRITE_PENDING);
    /* Drain queue */
    while (fde->disk.write_q) {
	q = fde->disk.write_q;
	fde->disk.write_q = q->next;
	if (q->free)
	    (q->free) (q->buf);
	safe_free(q);
    }
    fde->disk.write_q_tail = NULL;
    if (fde->disk.wrt_handle)
	fde->disk.wrt_handle(fd, DISK_ERROR, fde->disk.wrt_handle_data);
    commSetSelect(fd, COMM_SELECT_READ, NULL, NULL, 0);
    commSetSelect(fd, COMM_SELECT_WRITE, NULL, NULL, 0);
    file_close(fd);
}

/* close a disk file. */
void
file_close(int fd)
{
    FD_ENTRY *fde = NULL;
    if (fd < 0)
	fatal_dump("file_close: bad file number");
    fde = &fd_table[fd];
    if (!fde->open)
	fatal_dump("file_close: already closed");
    if (BIT_TEST(fde->flags, FD_WRITE_DAEMON)) {
	BIT_SET(fde->flags, FD_CLOSE_REQUEST);
	return;
    }
    if (BIT_TEST(fde->flags, FD_WRITE_PENDING)) {
	BIT_SET(fde->flags, FD_CLOSE_REQUEST);
	return;
    }
    fd_close(fd);
#if USE_ASYNC_IO
    aioClose(fd);
#else
    close(fd);
#endif
}


/* write handler */
static void
diskHandleWrite(int fd, void *unused)
{
    int len = 0;
    disk_ctrl_t *ctrlp;
    dwrite_q *q = NULL;
    dwrite_q *wq = NULL;
    FD_ENTRY *fde = &fd_table[fd];
    if (!fde->disk.write_q)
	return;
    if (!BIT_TEST(fde->flags, FD_AT_EOF))
	lseek(fd, 0, SEEK_END);
    /* We need to combine subsequent write requests after the first */
    if (fde->disk.write_q->next != NULL && fde->disk.write_q->next->next != NULL) {
	for (len = 0, q = fde->disk.write_q->next; q != NULL; q = q->next)
	    len += q->len - q->cur_offset;
	wq = xcalloc(1, sizeof(dwrite_q));
	wq->buf = xmalloc(len);
	wq->len = 0;
	wq->cur_offset = 0;
	wq->next = NULL;
	wq->free = xfree;
	do {
	    q = fde->disk.write_q->next;
	    len = q->len - q->cur_offset;
	    memcpy(wq->buf + wq->len, q->buf + q->cur_offset, len);
	    wq->len += len;
	    fde->disk.write_q->next = q->next;
	    if (q->free)
		(q->free) (q->buf);
	    safe_free(q);
	} while (fde->disk.write_q->next != NULL);
	fde->disk.write_q_tail = wq;
	fde->disk.write_q->next = wq;
    }
    ctrlp = xcalloc(1, sizeof(disk_ctrl_t));
    ctrlp->fd = fd;
#if USE_ASYNC_IO
    aioWrite(fd,
	fde->disk.write_q->buf + fde->disk.write_q->cur_offset,
	fde->disk.write_q->len - fde->disk.write_q->cur_offset,
	diskHandleWriteComplete,
	ctrlp);
#else
    len = write(fd,
	fde->disk.write_q->buf + fde->disk.write_q->cur_offset,
	fde->disk.write_q->len - fde->disk.write_q->cur_offset);
    diskHandleWriteComplete(ctrlp, len, errno);
#endif
}

static int
diskHandleWriteComplete(void *data, int len, int errcode)
{
    disk_ctrl_t *ctrlp = data;
    int fd = ctrlp->fd;
    FD_ENTRY *fde = &fd_table[fd];
    dwrite_q *q = fde->disk.write_q;
    errno = errcode;
    safe_free(data);
    fd_bytes(fd, len, FD_WRITE);
    if (q == NULL)		/* Someone aborted and then the write */
	return DISK_ERROR;	/* completed anyway. :( */
    BIT_SET(fde->flags, FD_AT_EOF);
    if (len < 0) {
	if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
	    len = 0;
	} else {
	    /* disk i/o failure--flushing all outstanding writes  */
	    debug(50, 1, "diskHandleWrite: FD %d: disk write error: %s\n",
		fd, xstrerror());
	    BIT_RESET(fde->flags, FD_WRITE_DAEMON);
	    BIT_RESET(fde->flags, FD_WRITE_PENDING);
	    /* call finish handler */
	    do {
		fde->disk.write_q = q->next;
		if (q->free)
		    (q->free) (q->buf);
		safe_free(q);
	    } while ((q = fde->disk.write_q));
	    if (fde->disk.wrt_handle) {
		fde->disk.wrt_handle(fd,
		    errno == ENOSPC ? DISK_NO_SPACE_LEFT : DISK_ERROR,
		    fde->disk.wrt_handle_data);
	    }
	    return DISK_ERROR;
	}
    }
    q->cur_offset += len;
    if (q->cur_offset > q->len)
	fatal_dump("diskHandleWriteComplete: offset > len");
    if (q->cur_offset == q->len) {
	/* complete write */
	fde->disk.write_q = q->next;
	if (q->free)
	    (q->free) (q->buf);
	safe_free(q);
    }
    if (fde->disk.write_q != NULL) {
	/* another block is queued */
	commSetSelect(fd,
	    COMM_SELECT_WRITE,
	    diskHandleWrite,
	    NULL,
	    0);
	return DISK_OK;
    }
    /* no more data */
    fde->disk.write_q = fde->disk.write_q_tail = NULL;
    BIT_RESET(fde->flags, FD_WRITE_PENDING);
    BIT_RESET(fde->flags, FD_WRITE_DAEMON);
    if (fde->disk.wrt_handle)
	fde->disk.wrt_handle(fd, DISK_OK, fde->disk.wrt_handle_data);
    if (BIT_TEST(fde->flags, FD_CLOSE_REQUEST))
	file_close(fd);
    return DISK_OK;
}


/* write block to a file */
/* write back queue. Only one writer at a time. */
/* call a handle when writing is complete. */
int
file_write(int fd,
    char *ptr_to_buf,
    int len,
    FILE_WRITE_HD handle,
    void *handle_data,
    void (*free_func) _PARAMS((void *)))
{
    dwrite_q *wq = NULL;
    FD_ENTRY *fde;
    if (fd < 0)
	fatal_dump("file_write: bad FD");
    fde = &fd_table[fd];
    if (!fde->open) {
	debug_trap("file_write: FILE_NOT_OPEN");
	return DISK_ERROR;
    }
    /* if we got here. Caller is eligible to write. */
    wq = xcalloc(1, sizeof(dwrite_q));
    wq->buf = ptr_to_buf;
    wq->len = len;
    wq->cur_offset = 0;
    wq->next = NULL;
    wq->free = free_func;
    fde->disk.wrt_handle = handle;
    fde->disk.wrt_handle_data = handle_data;

    /* add to queue */
    BIT_SET(fde->flags, FD_WRITE_PENDING);
    if (!(fde->disk.write_q)) {
	/* empty queue */
	fde->disk.write_q = fde->disk.write_q_tail = wq;
    } else {
	fde->disk.write_q_tail->next = wq;
	fde->disk.write_q_tail = wq;
    }

    if (!BIT_TEST(fde->flags, FD_WRITE_DAEMON)) {
#if USE_ASYNC_IO
	diskHandleWrite(fd, NULL);
#else
	commSetSelect(fd, COMM_SELECT_WRITE, diskHandleWrite, NULL, 0);
#endif
	BIT_SET(fde->flags, FD_WRITE_DAEMON);
    }
    return DISK_OK;
}



/* Read from FD */
static void
diskHandleRead(int fd, void *data)
{
    FD_ENTRY *fde = &fd_table[fd];
    dread_ctrl *ctrl_dat = data;
    int len;
    disk_ctrl_t *ctrlp;
    ctrlp = xcalloc(1, sizeof(disk_ctrl_t));
    ctrlp->fd = fd;
    ctrlp->data = ctrl_dat;
    /* go to requested position. */
    lseek(fd, ctrl_dat->offset, SEEK_SET);
    BIT_RESET(fde->flags, FD_AT_EOF);
#if USE_ASYNC_IO
    aioRead(fd,
	ctrl_dat->buf + ctrl_dat->cur_len,
	ctrl_dat->req_len - ctrl_dat->cur_len,
	diskHandleReadComplete,
	ctrlp);
#else
    len = read(fd,
	ctrl_dat->buf + ctrl_dat->cur_len,
	ctrl_dat->req_len - ctrl_dat->cur_len);
    diskHandleReadComplete(ctrlp, len, errno);
#endif
}

static int
diskHandleReadComplete(void *data, int len, int errcode)
{
    disk_ctrl_t *ctrlp = data;
    dread_ctrl *ctrl_dat = ctrlp->data;
    int fd = ctrlp->fd;
    errno = errcode;
    xfree(data);
    fd_bytes(fd, len, FD_READ);
    if (len < 0) {
	if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
	    commSetSelect(fd,
		COMM_SELECT_READ,
		diskHandleRead,
		ctrl_dat,
		0);
	    return DISK_OK;
	}
	debug(50, 1, "diskHandleRead: FD %d: error reading: %s\n",
	    fd, xstrerror());
	ctrl_dat->handler(fd, ctrl_dat->buf,
	    ctrl_dat->cur_len,
	    DISK_ERROR,
	    ctrl_dat->client_data);
	safe_free(ctrl_dat);
	return DISK_ERROR;
    } else if (len == 0) {
	/* EOF */
	ctrl_dat->end_of_file = 1;
	/* call handler */
	ctrl_dat->handler(fd,
	    ctrl_dat->buf,
	    ctrl_dat->cur_len,
	    DISK_EOF,
	    ctrl_dat->client_data);
	safe_free(ctrl_dat);
	return DISK_OK;
    } else {
	ctrl_dat->cur_len += len;
	ctrl_dat->offset = lseek(fd, 0L, SEEK_CUR);
    }
    /* reschedule if need more data. */
    if (ctrl_dat->cur_len < ctrl_dat->req_len) {
	commSetSelect(fd,
	    COMM_SELECT_READ,
	    diskHandleRead,
	    ctrl_dat,
	    0);
	return DISK_OK;
    } else {
	/* all data we need is here. */
	/* call handler */
	ctrl_dat->handler(fd,
	    ctrl_dat->buf,
	    ctrl_dat->cur_len,
	    DISK_OK,
	    ctrl_dat->client_data);
	safe_free(ctrl_dat);
	return DISK_OK;
    }
}


/* start read operation */
/* buffer must be allocated from the caller. 
 * It must have at least req_len space in there. 
 * call handler when a reading is complete. */
int
file_read(int fd, char *buf, int req_len, int offset, FILE_READ_HD * handler, void *client_data)
{
    dread_ctrl *ctrl_dat;
    if (fd < 0)
	fatal_dump("file_read: bad FD");
    ctrl_dat = xcalloc(1, sizeof(dread_ctrl));
    ctrl_dat->fd = fd;
    ctrl_dat->offset = offset;
    ctrl_dat->req_len = req_len;
    ctrl_dat->buf = buf;
    ctrl_dat->cur_len = 0;
    ctrl_dat->end_of_file = 0;
    ctrl_dat->handler = handler;
    ctrl_dat->client_data = client_data;
#if USE_ASYNC_IO
    diskHandleRead(fd, ctrl_dat);
#else
    commSetSelect(fd,
	COMM_SELECT_READ,
	diskHandleRead,
	ctrl_dat,
	0);
#endif
    return DISK_OK;
}


/* Read from FD and pass a line to routine. Walk to EOF. */
static void
diskHandleWalk(int fd, void *data)
{
    FD_ENTRY *fde = &fd_table[fd];
    dwalk_ctrl *walk_dat = data;
    int len;
    disk_ctrl_t *ctrlp;
    ctrlp = xcalloc(1, sizeof(disk_ctrl_t));
    ctrlp->fd = fd;
    ctrlp->data = walk_dat;
    lseek(fd, walk_dat->offset, SEEK_SET);
    BIT_RESET(fde->flags, FD_AT_EOF);
#if USE_ASYNC_IO
    aioRead(fd, walk_dat->buf,
	DISK_LINE_LEN - 1,
	diskHandleWalkComplete,
	ctrlp);
#else
    len = read(fd, walk_dat->buf, DISK_LINE_LEN - 1);
    diskHandleWalkComplete(ctrlp, len, errno);
#endif
}


static int
diskHandleWalkComplete(void *data, int retcode, int errcode)
{
    disk_ctrl_t *ctrlp = (disk_ctrl_t *) data;
    dwalk_ctrl *walk_dat;
    int fd;
    int len;
    LOCAL_ARRAY(char, temp_line, DISK_LINE_LEN);
    int end_pos;
    int st_pos;
    int used_bytes;

    walk_dat = (dwalk_ctrl *) ctrlp->data;
    fd = ctrlp->fd;
    len = retcode;
    errno = errcode;
    xfree(data);

    if (len < 0) {
	if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
	    commSetSelect(fd, COMM_SELECT_READ, diskHandleWalk, walk_dat, 0);
	    return DISK_OK;
	}
	debug(50, 1, "diskHandleWalk: FD %d: error readingd: %s\n",
	    fd, xstrerror());
	walk_dat->handler(fd, DISK_ERROR, walk_dat->client_data);
	safe_free(walk_dat->buf);
	safe_free(walk_dat);
	return DISK_ERROR;
    } else if (len == 0) {
	/* EOF */
	walk_dat->handler(fd, DISK_EOF, walk_dat->client_data);
	safe_free(walk_dat->buf);
	safe_free(walk_dat);
	return DISK_OK;
    }
    /* emulate fgets here. Cut the into separate line. newline is excluded */
    /* it throws last partial line, if exist, away. */
    used_bytes = st_pos = end_pos = 0;
    while (end_pos < len) {
	if (walk_dat->buf[end_pos] == '\n') {
	    /* new line found */
	    xstrncpy(temp_line, walk_dat->buf + st_pos, end_pos - st_pos + 1);
	    used_bytes += end_pos - st_pos + 1;

	    /* invoke line handler */
	    walk_dat->line_handler(fd, temp_line, strlen(temp_line),
		walk_dat->line_data);

	    /* skip to next line */
	    st_pos = end_pos + 1;
	}
	end_pos++;
    }

    /* update file pointer to the next to be read character */
    walk_dat->offset += used_bytes;

    /* reschedule it for next line. */
    commSetSelect(fd, COMM_SELECT_READ, diskHandleWalk, walk_dat, 0);
    return DISK_OK;
}


/* start walk through whole file operation 
 * read one block and chop it to a line and pass it to provided 
 * handler one line at a time.
 * call a completion handler when done. */
int
file_walk(int fd,
    FILE_WALK_HD * handler,
    void *client_data,
    FILE_WALK_LHD * line_handler,
    void *line_data)
{
    dwalk_ctrl *walk_dat;

    walk_dat = xcalloc(1, sizeof(dwalk_ctrl));
    walk_dat->fd = fd;
    walk_dat->offset = 0;
    walk_dat->buf = xcalloc(1, DISK_LINE_LEN);
    walk_dat->cur_len = 0;
    walk_dat->handler = handler;
    walk_dat->client_data = client_data;
    walk_dat->line_handler = line_handler;
    walk_dat->line_data = line_data;

#if USE_ASYNC_IO
    diskHandleWalk(fd, walk_dat);
#else
    commSetSelect(fd, COMM_SELECT_READ, diskHandleWalk, walk_dat, 0);
#endif
    return DISK_OK;
}

int
diskWriteIsComplete(int fd)
{
    return fd_table[fd].disk.write_q ? 0 : 1;
}

/*
 * $Id$
 *
 * DEBUG: section 6     Disk I/O Routines
 * AUTHOR: Harvest Derived
 *
 * SQUID Internet Object Cache  http://www.nlanr.net/Squid/
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

typedef struct _dwalk_ctrl {
    int fd;
    off_t offset;
    char *buf;			/* line buffer */
    int cur_len;		/* line len */
    int (*handler) _PARAMS((int fd, int errflag, void *data));
    void *client_data;
    int (*line_handler) _PARAMS((int fd, char *buf, int size, void *line_data));
    void *line_data;
} dwalk_ctrl;

/* table for FILE variable, write lock and queue. Indexed by fd. */
FileEntry *file_table;

/* initialize table */
int disk_init()
{
    int fd;

    file_table = xcalloc(FD_SETSIZE, sizeof(FileEntry));
    meta_data.misc += FD_SETSIZE * sizeof(FileEntry);
    for (fd = 0; fd < FD_SETSIZE; fd++) {
	file_table[fd].filename[0] = '\0';
	file_table[fd].at_eof = NO;
	file_table[fd].open_stat = NOT_OPEN;
	file_table[fd].close_request = NOT_REQUEST;
	file_table[fd].write_daemon = NOT_PRESENT;
	file_table[fd].write_lock = UNLOCK;
	file_table[fd].access_code = 0;
	file_table[fd].write_pending = NO_WRT_PENDING;
	file_table[fd].write_q = file_table[fd].write_q_tail = NULL;
    }
    return 0;
}

/* Open a disk file. Return a file descriptor */
int file_open(path, handler, mode)
     char *path;		/* path to file */
     int (*handler) ();		/* Interrupt handler. */
     int mode;
{
    FD_ENTRY *conn;
    int fd;

    /* Open file */
    if ((fd = open(path, mode | O_NDELAY, 0644)) < 0) {
	debug(6, 0, "file_open: error opening file %s: %s\n",
	    path, xstrerror());
	return (DISK_ERROR);
    }
    /* update fdstat */
    fdstat_open(fd, FD_FILE);

    /* init table */
    strncpy(file_table[fd].filename, path, MAX_FILE_NAME_LEN);
    file_table[fd].at_eof = NO;
    file_table[fd].open_stat = OPEN;
    file_table[fd].close_request = NOT_REQUEST;
    file_table[fd].write_lock = UNLOCK;
    file_table[fd].write_pending = NO_WRT_PENDING;
    file_table[fd].write_daemon = NOT_PRESENT;
    file_table[fd].access_code = 0;
    file_table[fd].write_q = NULL;

    conn = &fd_table[fd];
    memset(conn, '\0', sizeof(FD_ENTRY));
    if (commSetNonBlocking(fd) == COMM_ERROR)
	return DISK_ERROR;
    conn->comm_type = COMM_NONBLOCKING;
    return fd;
}

int file_update_open(fd, path)
     int fd;
     char *path;		/* path to file */
{
    FD_ENTRY *conn;

    /* update fdstat */
    fdstat_open(fd, FD_FILE);

    /* init table */
    strncpy(file_table[fd].filename, path, MAX_FILE_NAME_LEN);
    file_table[fd].at_eof = NO;
    file_table[fd].open_stat = OPEN;
    file_table[fd].close_request = NOT_REQUEST;
    file_table[fd].write_lock = UNLOCK;
    file_table[fd].write_pending = NO_WRT_PENDING;
    file_table[fd].write_daemon = NOT_PRESENT;
    file_table[fd].access_code = 0;
    file_table[fd].write_q = NULL;

    conn = &fd_table[fd];
    memset(conn, '\0', sizeof(FD_ENTRY));
    conn->comm_type = COMM_NONBLOCKING;
    return fd;
}


/* close a disk file. */
int file_close(fd)
     int fd;			/* file descriptor */
{
    FD_ENTRY *conn = NULL;

    /* we might have to flush all the write back queue before we can
     * close it */
    /* save it for later */

    if (file_table[fd].open_stat == NOT_OPEN) {
	debug(6, 3, "file_close: FD %d is not OPEN\n", fd);
    } else if (file_table[fd].write_daemon == PRESENT) {
	debug(6, 3, "file_close: FD %d has a write daemon PRESENT\n", fd);
    } else if (file_table[fd].write_pending == WRT_PENDING) {
	debug(6, 3, "file_close: FD %d has a write PENDING\n", fd);
    } else {
	file_table[fd].open_stat = NOT_OPEN;
	file_table[fd].write_lock = UNLOCK;
	file_table[fd].write_daemon = NOT_PRESENT;
	file_table[fd].filename[0] = '\0';

	if (fdstat_type(fd) == FD_SOCKET) {
	    debug(6, 0, "FD %d: Someone called file_close() on a socket\n", fd);
	    fatal_dump(NULL);
	}
	/* update fdstat */
	fdstat_close(fd);
	conn = &fd_table[fd];
	memset(conn, '\0', sizeof(FD_ENTRY));
	comm_set_fd_lifetime(fd, -1);	/* invalidate the lifetime */
	close(fd);
	return DISK_OK;
    }

    /* refused to close file if there is a daemon running */
    /* have pending flag set */
    file_table[fd].close_request = REQUEST;
    return DISK_ERROR;
}

/* grab a writing lock for file */
int file_write_lock(fd)
     int fd;
{
    if (file_table[fd].write_lock == LOCK) {
	debug(6, 0, "trying to lock a locked file\n");
	return DISK_WRT_LOCK_FAIL;
    } else {
	file_table[fd].write_lock = LOCK;
	file_table[fd].access_code += 1;
	file_table[fd].access_code %= 65536;
	return file_table[fd].access_code;
    }
}


/* release a writing lock for file */
int file_write_unlock(fd, access_code)
     int fd;
     int access_code;
{
    if (file_table[fd].access_code == access_code) {
	file_table[fd].write_lock = UNLOCK;
	return DISK_OK;
    } else {
	debug(6, 0, "trying to unlock the file with the wrong access code\n");
	return DISK_WRT_WRONG_CODE;
    }
}


/* write handler */
int diskHandleWrite(fd, entry)
     int fd;
     FileEntry *entry;
{
    int len;
    dwrite_q *q;
    int block_complete = 0;

    if (file_table[fd].at_eof == NO)
	lseek(fd, 0, SEEK_END);

    for (;;) {
	len = write(fd, (entry->write_q->buf) + entry->write_q->cur_offset,
	    entry->write_q->len - entry->write_q->cur_offset);

	file_table[fd].at_eof = YES;

	if (len < 0) {
	    switch (errno) {
#if EAGAIN != EWOULDBLOCK
	    case EAGAIN:
#endif
	    case EWOULDBLOCK:
		/* just reschedule itself, try again */
		comm_set_select_handler(fd,
		    COMM_SELECT_WRITE,
		    (PF) diskHandleWrite,
		    (void *) entry);
		entry->write_daemon = PRESENT;
		return DISK_OK;
	    default:
		/* disk i/o failure--flushing all outstanding writes  */
		debug(6, 1, "diskHandleWrite: FD %d: disk write error: %s\n",
		    fd, xstrerror());
		entry->write_daemon = NOT_PRESENT;
		entry->write_pending = NO_WRT_PENDING;
		/* call finish handler */
		do {
		    q = entry->write_q;
		    entry->write_q = q->next;
		    if (!entry->wrt_handle) {
			safe_free(q->buf);
		    } else {
			/* XXXXXX 
			 * Notice we call the handler multiple times but
			 * the write handler (in page mode) doesn't know
			 * the buf ptr so it'll be hard to deallocate
			 * memory.
			 * XXXXXX */
			entry->wrt_handle(fd,
			    errno == ENOSPC ? DISK_NO_SPACE_LEFT : DISK_ERROR,
			    entry->wrt_handle_data);
		    }
		    safe_free(q);
		} while (entry->write_q);
		return DISK_ERROR;
	    }
	}
	entry->write_q->cur_offset += len;
	block_complete = (entry->write_q->cur_offset >= entry->write_q->len);

	if (block_complete && (!entry->write_q->next)) {
	    /* No more data */
	    if (!entry->wrt_handle)
		safe_free(entry->write_q->buf);
	    safe_free(entry->write_q);
	    entry->write_q = entry->write_q_tail = NULL;
	    entry->write_pending = NO_WRT_PENDING;
	    entry->write_daemon = NOT_PRESENT;
	    /* call finish handle */
	    if (entry->wrt_handle) {
		entry->wrt_handle(fd, DISK_OK, entry->wrt_handle_data);
	    }
	    /* Close it if requested */
	    if (file_table[fd].close_request == REQUEST) {
		file_close(fd);
	    }
	    return DISK_OK;
	} else if ((block_complete) && (entry->write_q->next)) {
	    /* Do next block */

	    /* XXXXX THESE PRIMITIVES ARE WEIRD XXXXX   
	     * If we have multiple blocks to send, we  
	     * only call the completion handler once, 
	     * so it becomes our job to free buffer space    
	     */

	    q = entry->write_q;
	    entry->write_q = entry->write_q->next;
	    if (!entry->wrt_handle)
		safe_free(q->buf);
	    safe_free(q);
	    /* Schedule next write 
	     *  comm_set_select_handler(fd, COMM_SELECT_WRITE, (PF) diskHandleWrite,
	     *      (void *) entry);
	     */
	    entry->write_daemon = PRESENT;
	    /* Repeat loop */
	} else {		/* !Block_completed; block incomplete */
	    /* reschedule */
	    comm_set_select_handler(fd, COMM_SELECT_WRITE, (PF) diskHandleWrite,
		(void *) entry);
	    entry->write_daemon = PRESENT;
	    return DISK_OK;
	}
    }
}



/* write block to a file */
/* write back queue. Only one writer at a time. */
/* call a handle when writing is complete. */
int file_write(fd, ptr_to_buf, len, access_code, handle, handle_data)
     int fd;
     char *ptr_to_buf;
     int len;
     int access_code;
     void (*handle) ();
     void *handle_data;
{
    dwrite_q *wq;

    if (file_table[fd].open_stat != OPEN) {
	return DISK_ERROR;
    }
    if ((file_table[fd].write_lock == LOCK) &&
	(file_table[fd].access_code != access_code)) {
	debug(6, 0, "file write: FD %d access code checked failed.\n", fd);
	return DISK_WRT_WRONG_CODE;
    }
    /* if we got here. Caller is eligible to write. */
    wq = xcalloc(1, sizeof(dwrite_q));

    wq->buf = ptr_to_buf;

    wq->len = len;
    wq->cur_offset = 0;
    wq->next = NULL;
    file_table[fd].wrt_handle = handle;
    file_table[fd].wrt_handle_data = handle_data;

    /* add to queue */
    file_table[fd].write_pending = WRT_PENDING;
    if (!(file_table[fd].write_q)) {
	/* empty queue */
	file_table[fd].write_q = file_table[fd].write_q_tail = wq;

    } else {
	file_table[fd].write_q_tail->next = wq;
	file_table[fd].write_q_tail = wq;
    }

    if (file_table[fd].write_daemon == PRESENT)
	return DISK_OK;
    /* got to start write routine for this fd */
#if USE_ASYNC_IO
    return aioFileQueueWrite(fd,
	file_aio_write_complete,
	&file_table[fd]);
#else
    comm_set_select_handler(fd,
	COMM_SELECT_WRITE,
	(PF) diskHandleWrite,
	(void *) &file_table[fd]);
    return DISK_OK;
#endif
}



/* Read from FD */
int diskHandleRead(fd, ctrl_dat)
     int fd;
     dread_ctrl *ctrl_dat;
{
    int len;

    /* go to requested position. */
    lseek(fd, ctrl_dat->offset, SEEK_SET);
    file_table[fd].at_eof = NO;
    len = read(fd, ctrl_dat->buf + ctrl_dat->cur_len,
	ctrl_dat->req_len - ctrl_dat->cur_len);

    if (len < 0)
	switch (errno) {
#if EAGAIN != EWOULDBLOCK
	case EAGAIN:
#endif
	case EWOULDBLOCK:
	    break;
	default:
	    debug(6, 1, "diskHandleRead: FD %d: error reading: %s\n",
		fd, xstrerror());
	    ctrl_dat->handler(fd, ctrl_dat->buf,
		ctrl_dat->cur_len, DISK_ERROR,
		ctrl_dat->client_data, ctrl_dat->offset);
	    safe_free(ctrl_dat);
	    return DISK_ERROR;
    } else if (len == 0) {
	/* EOF */
	ctrl_dat->end_of_file = 1;
	/* call handler */
	ctrl_dat->handler(fd, ctrl_dat->buf, ctrl_dat->cur_len, DISK_EOF,
	    ctrl_dat->client_data, ctrl_dat->offset);
	safe_free(ctrl_dat);
	return DISK_OK;
    }
    ctrl_dat->cur_len += len;
    ctrl_dat->offset = lseek(fd, 0L, SEEK_CUR);

    /* reschedule if need more data. */
    if (ctrl_dat->cur_len < ctrl_dat->req_len) {
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) diskHandleRead,
	    (void *) ctrl_dat);
	return DISK_OK;
    } else {
	/* all data we need is here. */
	/* calll handler */
	ctrl_dat->handler(fd, ctrl_dat->buf, ctrl_dat->cur_len, DISK_OK,
	    ctrl_dat->client_data, ctrl_dat->offset);
	safe_free(ctrl_dat);
	return DISK_OK;
    }
}


/* start read operation */
/* buffer must be allocated from the caller. 
 * It must have at least req_len space in there. 
 * call handler when a reading is complete. */
int file_read(fd, buf, req_len, offset, handler, client_data)
     int fd;
     char *buf;
     int req_len;
     int offset;
     FILE_READ_HD handler;
     void *client_data;
{
    dread_ctrl *ctrl_dat;

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
    return aioFileQueueRead(fd, file_aio_read_complete, ctrl_dat);
#else
    comm_set_select_handler(fd,
	COMM_SELECT_READ,
	(PF) diskHandleRead,
	(void *) ctrl_dat);
    return DISK_OK;
#endif
}


/* Read from FD and pass a line to routine. Walk to EOF. */
int diskHandleWalk(fd, walk_dat)
     int fd;
     dwalk_ctrl *walk_dat;
{
    int len;
    int end_pos;
    int st_pos;
    int used_bytes;
    static char temp_line[DISK_LINE_LEN];

    lseek(fd, walk_dat->offset, SEEK_SET);
    file_table[fd].at_eof = NO;
    len = read(fd, walk_dat->buf, DISK_LINE_LEN - 1);

    if (len < 0)
	switch (errno) {
#if EAGAIN != EWOULDBLOCK
	case EAGAIN:
#endif
	case EWOULDBLOCK:
	    break;
	default:
	    debug(6, 1, "diskHandleWalk: FD %d: error readingd: %s\n",
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
	    strncpy(temp_line, walk_dat->buf + st_pos, end_pos - st_pos);
	    temp_line[end_pos - st_pos] = '\0';
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
    comm_set_select_handler(fd, COMM_SELECT_READ, (PF) diskHandleWalk,
	(void *) walk_dat);
    return DISK_OK;
}


/* start walk through whole file operation 
 * read one block and chop it to a line and pass it to provided 
 * handler one line at a time.
 * call a completion handler when done. */
int file_walk(fd, handler, client_data, line_handler, line_data)
     int fd;
     FILE_WALK_HD handler;
     void *client_data;
     FILE_WALK_LHD line_handler;
     void *line_data;

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

    comm_set_select_handler(fd, COMM_SELECT_READ, (PF) diskHandleWalk,
	(void *) walk_dat);
    return DISK_OK;
}

char *diskFileName(fd)
     int fd;
{
    if (file_table[fd].filename[0])
	return (file_table[fd].filename);
    else
	return (0);
}

/*
 * $Id$
 *
 * AUTHOR: Alex Rousskov
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

#ifndef _IO_BUFFER_H_
#define _IO_BUFFER_H_

/* a handy buffer that supports pseudo-concurrent read and write operations */
struct _IOBuffer {
    /* public, readable */
    size_t capacity;

    /* protected, do not use these, use interface functions instead */
    void *rlock;  /* points to the owner of Read lock  if any */
    void *wlock;  /* points to the owner of Write lock if any */
    void *block;  /* points to the process that uses buffer (reads/writes) _now_ */

    /* private, stay away */
    char *buf;    /* can be NULL when no data is present */
    size_t rsize; /* amount of readable data; other sizes/pointers are derived */
    int terminator_hack; /* see IOBuffer.c for details */
};

/* create/init/destroy */
extern IOBuffer *ioBufferCreate(size_t capacity, int term_hack);
extern void ioBufferInit(IOBuffer *iob, size_t capacity, int term_hack);
extern void ioBufferDestroy(IOBuffer *iob);

/* grow (the buffer should not be used when grow() called) */
extern void ioBufferGrow(IOBuffer *iob, void *writer, size_t new_capacity);

/*
 * Locks are for double checking that racing conditions do not exist. There are
 * two kinds of such conditions: (1) two concurrent readers (or writers) OR (2)
 * one reader and one writer doing ios at the same time. Locks do not block, of
 * course, but assert(!) because racing conditions should never exist for them
 * in a working program.
 */

/*
 * A lock is good for many operations on the buffer. That is, once obtained, it
 * allows to do as many ios as you want.  Racing conditions of the second kind
 * are checked using startIO/doneIO pairs.
 */

/* obtain lock (_asserts_ that lock is not set) */
extern void ioBufferRLock(IOBuffer *iob, void *reader);
extern void ioBufferWLock(IOBuffer *iob, void *writer);

/* release lock (_asserts_ that lock is set by releaser) */
extern void ioBufferRUnLock(IOBuffer *iob, void *reader);
extern void ioBufferWUnLock(IOBuffer *iob, void *writer);

/* start using: returns pointer and sets available size if sizep is supplied */
extern char *ioBufferStartReading(IOBuffer *iob, void *reader, size_t *sizep);
extern char *ioBufferStartWriting(IOBuffer *iob, void *writer, size_t *sizep);

/* stop using buffer: must specify how much you read or wrote */
extern void ioBufferDoneReading(IOBuffer *iob, void *reader, size_t size);
extern void ioBufferDoneWriting(IOBuffer *iob, void *writer, size_t size);

/* if you are lasy: start()+read/write+done(); locking is left for you */
extern size_t ioBufferRead(IOBuffer *iob, int fd, void *reader);
extern size_t ioBufferWrite(IOBuffer *iob, int fd, void *writer);

/*
 * inline functions to test buffer state;
 * do not require locking or start()/done() brackets 
 * note: these can become real functions at any time if needed
 */

/* test if there is data in the buffer */
#define ioBufferIsEmpty(iob) (!(iob) || !(iob)->rsize)

#endif /* ndef _IO_BUFFER_H_ */

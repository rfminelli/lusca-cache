/*
 * $Id$
 *
 * DEBUG: section ??    IO Buffer
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

/* To-do:
 *    - replace xmalloc/free with recyclable buffers
 */

/* General schema:

   <-----------------  capacity  ------------------>
   <---  rsize  ---->.<--------- "wsize" ---------->
   [-----------------|-----------------------------]
   \ "read_ptr"      \"write_ptr"                    

   Important:
	- "thing"s are not stored but calculated run-time
	- when reading is done(), we always shift buffer to the left
	  so read_ptr==buf when start() is called
	- when empty buffer is not in use, the buf pointer can be NULL
        - we could allow for concurrent reading and writing, but what for?
          (and it is much more safe this way)
*/

#include "squid.h"
#include "IOBuffer.h"

/* local constants */


/* local routines */

/* handy "inlines" for run-time calculated fields */
#define ioBufferRPtr(iob) ((iob)->buf) /* may return NULL is rsize is 0! */
#define ioBufferWPtr(iob) ((iob)->buf+(iob)->rsize)
#define ioBufferRSize(iob) ((iob)->rsize)
#define ioBufferWSize(iob) ((iob)->capacity-(iob)->rsize)

static void ioBufferExpand(IOBuffer *iob);
static void ioBufferCollapse(IOBuffer *iob);


IOBuffer *
ioBufferCreate(size_t capacity)
{
    IOBuffer *iob = xcalloc(sizeof(IOBuffer));
    iob->capacity = capacity;
    /* other fields are set to 0 by calloc */
    /* buffer area is allocated when it is actually needed */
    return iob;
}

void 
ioBufferDestroy(IOBuffer *iob) {
    assert(iob);
    assert(!iob->rlock && !iob->wlock && !iob->block);
    ioBufferCollapse(iob);
    xfree(iob);
}

/* grow (the buffer should not be "used" when grow() is called) */
void
ioBufferGrow(IOBuffer *iob, void *writer, size_t new_capacity)
{
    assert(iob);
    assert(writer && iob->wlock == writer && !iob->block);
    assert(new_capacity > iob->capacity);
    if (iob->buf) {
        iob->buf = xrealloc(iob->buf, new_capacity);
	meta_data.io_buffers -= iob->capacity;
	meta_data.io_buffers += new_capacity;
    }
    iob->capacity = new_capacity;
}

/* obtain lock (_asserts_ that lock is not set) */
void 
ioBufferRLock(IOBuffer *iob, void *reader)
{
    assert(iob);
    assert(reader && !iob->rlock);
    iob->rlock = reader;
}

/* obtain lock (_asserts_ that lock is not set) */
void 
ioBufferWLock(IOBuffer *iob, void *writer);
{
    assert(iob);
    assert(writer && !iob->wlock);
    iob->wlock = writer;
}

/* release lock (_asserts_ that lock is set by releaser) */
void 
ioBufferRUnLock(IOBuffer *iob, void *reader)
{
    assert(iob);
    assert(reader && iob->rlock == reader && iob->block != reader);
    iob->rlock = NULL;
}

/* release lock (_asserts_ that lock is set by releaser) */
void 
ioBufferWUnLock(IOBuffer *iob, void *writer)
{
    assert(iob);
    assert(writer && iob->wlock == writer && iob->block != writer);
    iob->wlock = NULL;
}

/* start using: returns pointer and sets available size if sizep is supplied */
char *
ioBufferStartReading(IOBuffer *iob, void *reader, size_t *sizep)
{
    assert(iob);
    assert(reader && iob->rlock == reader && !iob->block);
    iob->block = reader;
    /* note: no expand() */
    if (*sizep)
	*sizep = ioBufferRSize(iob);
    return ioBufferRPtr(iob);
}

/* start using: returns pointer and sets available size if sizep is supplied */
char *
ioBufferStartWriting(IOBuffer *iob, void *writer, size_t *sizep)
{
    assert(iob);
    assert(writer && iob->wlock == writer && !iob->block);
    iob->block = writer;
    if (!iob->buf)
	ioBufferExpand();
    if (*sizep)
	*sizep = ioBufferWSize(iob);
    return ioBufferWPtr(iob);
}

/* stop using buffer: must specify how much you read */
void 
ioBufferDoneReading(IOBuffer *iob, void *reader, size_t size);
{
    assert(iob);
    assert(reader && iob->rlock == reader && iob->block == reader);
    assert(size >= 0);
    iob->rsize -= size;
    assert(iob->rsize >= 0);
    if (!iob->rsize)
	ioBufferCollapse(iob); /* nothing to keep */
    else
	memmove(iob->buf, iob->buf+size, iob->rsize);
    iob->block = NULL;
}

/* stop using buffer: must specify how much you wrote */
void 
ioBufferDoneWriting(IOBuffer *iob, void *writer, size_t size)
{
    assert(iob);
    assert(writer && iob->wlock == writer && iob->block == writer);
    assert(size >= 0);
    iob->rsize += size;
    assert(iob->rsize <= iob->capacity);
    if (!iob->rsize)
	ioBufferCollapse(iob); /* nothing to keep */
    iob->block = NULL;
}

/* if you are lasy: start()+read+done(); locking is left for you */
size_t
ioBufferRead(IOBuffer *iob, int fd, void *reader)
{
    size_t size = 0;
    char *buf = ioBufferStartReading(iob, reader, &size);
    if (size > 0) {
	size = read(fd, buf, size);
	if (size > 0)
	    fd_bytes(fd, size, FD_READ);
    }
    ioBufferDoneReading(iob, reader, size > 0 ? size : 0);
    return size;
}

/* if you are lasy: start()+write+done(); locking is left for you */
size_t
ioBufferWrite(IOBuffer *iob, int fd, void *writer);
{
    size_t size = 0;
    char *buf = ioBufferStartWriting(iob, writer, &size);
    if (size > 0) {
	size = write(fd, buf, size);
	if (size > 0)
	    fd_bytes(fd, size, FD_WRITE);
    }
    ioBufferDoneWriting(iob, writer, size > 0 ? size : 0);
    return size;
}



/*
 * internal functions; all checks must be made before calling these
 */

/* see also ioBufferGrow() if you change memory accounting here     */

static void
ioBufferCollapse(IOBuffer *iob)
{
    if (iob->buf) {
        xfree(iob->buf);
        iob->buf = NULL;
	meta_data.io_buffers -= iob->capacity;
    }
}

static void
ioBufferExpand(IOBuffer *iob)
{
    if (!iob->buf) {
        iob->buf = xcalloc(iob->capacity);
	meta_data.io_buffers += iob->capacity;
    }
}

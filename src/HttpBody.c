/*
 * $Id$
 *
 * DEBUG: section ??    HTTP Body
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

#include "squid.h"


/* local constants */

/* local routines */


void
httpBodyInit(HttpBody *body)
{
    body->buf = NULL;
    body->packed_size = 1; /* one terminating character */
}

void
httpBodyClean(HttpBody *body)
{
    assert(body);
    safe_free(body->buf);
    body->buf = NULL;
    body->packed_size = 0;
}

void
httpBodySet(HttpBody *body, const char *buf, int size)
{
    assert(body);
    assert(!body->buf);
    assert(size);
    assert(buf[size-1] == '\0'); /* paranoid */
    body->buf = xmalloc(size);
    xmemcpy(body->buf, buf, size);
    body->packed_size = size;
}

void
httpBodySwap(const HttpBody *body, StoreEntry *entry)
{
    assert(body);
    storeAppend(entry, httpBodyPtr(body), body->packed_size);
}

const char *
httpBodyPtr(const HttpBody *body)
{
    return body->buf ? body->buf : "";
}

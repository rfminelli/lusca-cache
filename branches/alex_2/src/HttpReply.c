/*
 * $Id$
 *
 * DEBUG: section ??    HTTP Reply
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


HttpReply *
httpReplyCreate()
{
    HttpReply *rep = memAllocate(MEM_HTTPREPLY, 1);
    /* init with invalid values */
    rep->date = -2;
    rep->expires = -2;
    rep->last_modified = -2;
    rep->content_length = -1;
    return rep;
}

void
httpReplyDestroy(HttpReply *rep)
{
    assert(rep);
    memFree(MEM_HTTPREPLY, rep);
}

void
httpReplyUpdateOnNotModified(HttpReply *rep, HttpReply *freshRep)
{
    rep->cache_control = freshRep->cache_control;
    rep->misc_headers = freshRep->misc_headers;
    if (freshRep->date > -1)
	rep->date = freshRep->date;
    if (freshRep->last_modified > -1)
	rep->last_modified = freshRep->last_modified;
    if (freshRep->expires > -1)
	rep->expires = freshRep->expires;
}

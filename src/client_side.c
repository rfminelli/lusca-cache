/*
 * $Id$
 *
 * DEBUG: section 33    Client-side Routines
 * AUTHOR: Duane Wessels
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

/* Local constants */

/* Local functions */


/* Accept a new connection on HTTP socket */
void
clientAccept(int sock, void *notused)
{
    /* re-register this handler */
    commSetSelect(sock, COMM_SELECT_READ, httpAccept, NULL, 0);
    if (!httpConnAccept(sock)) {
    	debug(12, 1) ("httpAccept: FD %d: accept failure: %s\n",
	    sock, xstrerror());
	return; /* nothing left to be done */
    }
    debug(12, 4) ("httpAccept: FD %d: accepted\n", fd);
    /* register this connection on this side @?@ add this? */
}

/* send reply to a client (with possible queueing at httpConn) */
void
clientSendReply(Reply *rep)
{
    assert(rep);
    assert(rep->request);
    assert(rep->request->conn);
    httpConnWriteReply(rep->request->conn, rep);
}

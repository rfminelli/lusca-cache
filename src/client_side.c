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
#include "HttpConn.h" /* @?@ -> structs.h */
#include "HttpRequest.h"  /* @?@ -> structs.h */
#include "HttpReply.h"    /* @?@ -> structs.h */

/* Local constants */

/* Local functions */
static void clientAccept(int sock, void *notused);
static void checkFailureRatio(err_type, hier_code);

/* Accept a new connection on HTTP socket */
static void
clientAccept(int sock, void *notused)
{
    /* re-register this handler */
    commSetSelect(sock, COMM_SELECT_READ, httpAccept, NULL, 0);
    if (!httpConnAccept(sock)) {
    	debug(12, 1) ("clientAccept: FD %d: accept failure: %s\n",
	    sock, xstrerror());
	return; /* nothing left to be done */
    }
    /* register this connection on this side @?@ add this? */
}

/* send reply to a client (with possible queueing at httpConn) */
void
clientSendReply(HttpReply *rep)
{
    assert(rep);
    assert(rep->request);
    assert(rep->request->conn);
    httpConnSendReply(rep->request->conn, rep);
    checkFailureRatio(rep->err_type, rep->al.hier.code);
}

void
clientHttpConnectionsOpen()
{
    ushortlist *u;
    int fd;
    for (u = Config.Port.http; u; u = u->next) {
	enter_suid();
	fd = comm_open(SOCK_STREAM,
	    0,
	    Config.Addrs.tcp_incoming,
	    u->i,
	    COMM_NONBLOCKING,
	    "HTTP Socket");
	leave_suid();
	if (fd < 0)
	    continue;
	comm_listen(fd);
	commSetSelect(fd, COMM_SELECT_READ, clientAccept, NULL, 0);
	commSetDefer(fd, httpAcceptDefer, NULL);
	debug(1, 1) ("Accepting HTTP connections on port %d, FD %d.\n",
	    (int) u->i, fd);
	HttpSockets[NHttpSockets++] = fd;
    }
    if (NHttpSockets < 1)
	fatal("Cannot open HTTP Port");
}

void
clientHttpConnectionsClose()
{
    int i;
    for (i = 0; i < NHttpSockets; i++) {
	if (HttpSockets[i] >= 0) {
	    debug(1, 1) ("FD %d Closing HTTP connection\n", HttpSockets[i]);
	    comm_close(HttpSockets[i]);
	    HttpSockets[i] = -1;
	}
    }
    NHttpSockets = 0;
}

/*
 * This function is designed to serve a fairly specific purpose.
 * Occasionally our vBNS-connected caches can talk to each other, but not
 * the rest of the world.  Here we try to detect frequent failures which
 * make the cache unusable (e.g. DNS lookup and connect() failures).  If
 * the failure:success ratio goes above 1.0 then we go into "hit only"
 * mode where we only return UDP_HIT or UDP_MISS_NOFETCH.  Neighbors
 * will only fetch HITs from us if they are using the ICP protocol.  We
 * stay in this mode for 5 minutes.
 * 
 * Duane W., Sept 16, 1996
 */

static void
checkFailureRatio(err_type etype, hier_code hcode)
{
    enum { FAILURE_MODE_TIME = 300 }; /* local constant */
    static double magic_factor = 100.0;
    double n_good;
    double n_bad;
    if (hcode == HIER_NONE)
	return;
    n_good = magic_factor / (1.0 + request_failure_ratio);
    n_bad = magic_factor - n_good;
    switch (etype) {
    case ERR_DNS_FAIL:
    case ERR_CONNECT_FAIL:
    case ERR_READ_ERROR:
	n_bad++;
	break;
    default:
	n_good++;
    }
    request_failure_ratio = n_bad / n_good;
    if (hit_only_mode_until > squid_curtime)
	return;
    if (request_failure_ratio < 1.0)
	return;
    debug(12, 0) ("Failure Ratio at %4.2f\n", request_failure_ratio);
    debug(12, 0) ("Going into hit-only-mode for %d minutes...\n",
	FAILURE_MODE_TIME / 60);
    hit_only_mode_until = squid_curtime + FAILURE_MODE_TIME;
    request_failure_ratio = 0.8;        /* reset to something less than 1.0 */
}


/*
 * $Id$
 *
 * DEBUG: section 80     WCCP Support
 * AUTHOR: Glenn Chisholm
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by the
 *  National Science Foundation.  Squid is Copyrighted (C) 1998 by
 *  Duane Wessels and the University of California San Diego.  Please
 *  see the COPYRIGHT file for full details.  Squid incorporates
 *  software developed and/or copyrighted by other sources.  Please see
 *  the CREDITS file for full details.
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

#if USE_WCCP

#define WCCP_PORT 2048
#define WCCP_VERSION 4
#define WCCP_REVISION 0
#define WCCP_RESPONSE_SIZE 12448
#define WCCP_ACTIVE_CACHES 32
#define WCCP_HASH_SIZE 32
#define WCCP_BUCKETS 256

#define WCCP_HERE_I_AM 7
#define WCCP_I_SEE_YOU 8
#define WCCP_ASSIGN_BUCKET 9

struct wccp_here_i_am_t {
    int type;
    int version;
    int revision;
    char hash[WCCP_HASH_SIZE];
    int reserved;
    int id;
};

struct wccp_cache_entry_t {
    int ip_addr;
    int revision;
    char hash[WCCP_HASH_SIZE];
    int reserved;
};

struct wccp_i_see_you_t {
    int type;
    int version;
    int change;
    int id;
    int number;
    struct wccp_cache_entry_t wccp_cache_entry[WCCP_ACTIVE_CACHES];
};

struct wccp_assign_bucket_t {
    int type;
    int id;
    int number;
};

static int theInWccpConnection = -1;
static int theOutWccpConnection = -1;
static struct wccp_here_i_am_t wccp_here_i_am;
static struct wccp_i_see_you_t wccp_i_see_you;
static int change, local_ip;

static int wccpLowestIP();

/*
 * The functions used during startup:
 * wccpInit
 * wccpConnectionOpen
 * wccpConnectionShutdown
 * wccpConnectionClose
 */

void
wccpInit(void)
{
    debug(80, 5) ("wccpInit: Called\n");

    memset(&wccp_here_i_am, '\0', sizeof(wccp_here_i_am));
    wccp_here_i_am.type = htonl(WCCP_HERE_I_AM);
    wccp_here_i_am.version = htonl(WCCP_VERSION);
    wccp_here_i_am.revision = htonl(WCCP_REVISION);

    change = 1;

}

void
wccpConnectionOpen(void)
{
    u_short port = WCCP_PORT;
    struct sockaddr_in router, local;
    int local_len, router_len;

    debug(80, 5) ("wccpConnectionOpen: Called\n");
    if (Config.Wccp.router.s_addr != inet_addr("0.0.0.0")) {
	theInWccpConnection = comm_open(SOCK_DGRAM,
	    0,
	    Config.Wccp.incoming,
	    port,
	    COMM_NONBLOCKING,
	    "WCCP Port");
	if (theInWccpConnection < 0)
	    fatal("Cannot open WCCP Port");
	commSetSelect(theInWccpConnection, COMM_SELECT_READ, wccpHandleUdp, NULL, 0);
	debug(1, 1) ("Accepting WCCP messages on port %d, FD %d.\n",
	    (int) port, theInWccpConnection);
	if (Config.Wccp.outgoing.s_addr != no_addr.s_addr) {
	    theOutWccpConnection = comm_open(SOCK_DGRAM,
		0,
		Config.Wccp.outgoing,
		port,
		COMM_NONBLOCKING,
		"WCCP Port");
	    if (theOutWccpConnection < 0)
		fatal("Cannot open Outgoing WCCP Port");
	    commSetSelect(theOutWccpConnection,
		COMM_SELECT_READ,
		wccpHandleUdp,
		NULL, 0);
	    debug(1, 1) ("Outgoing WCCP messages on port %d, FD %d.\n",
		(int) port, theOutWccpConnection);
	    fd_note(theOutWccpConnection, "Outgoing WCCP socket");
	    fd_note(theInWccpConnection, "Incoming WCCP socket");
	} else {
	    theOutWccpConnection = theInWccpConnection;
	}
    } else {
	debug(1, 1) ("WCCP Disabled.\n");
    }

    router_len = sizeof(router);
    memset(&router, '\0', router_len);
    router.sin_family = AF_INET;
    router.sin_port = htons(2048);
    router.sin_addr = Config.Wccp.router;
    if (connect(theOutWccpConnection, (struct sockaddr *) &router, router_len))
	fatal("Unable to connect WCCP out socket");

    local_len = sizeof(local);
    memset(&local, '\0', local_len);
    if (getsockname(theOutWccpConnection, (struct sockaddr *) &local, &local_len))
	fatal("Unable to getsockname on WCCP out socket");
    local_ip = local.sin_addr.s_addr;
}

void
wccpConnectionShutdown(void)
{
    if (theInWccpConnection < 0)
	return;
    if (theInWccpConnection != theOutWccpConnection) {
	debug(80, 1) ("FD %d Closing WCCP socket\n", theInWccpConnection);
	comm_close(theInWccpConnection);
    }
    assert(theOutWccpConnection > -1);
    commSetSelect(theOutWccpConnection, COMM_SELECT_READ, NULL, NULL, 0);
}

void
wccpConnectionClose(void)
{
    wccpConnectionShutdown();
    if (theOutWccpConnection > -1) {
	debug(80, 1) ("FD %d Closing WCCP socket\n", theOutWccpConnection);
	comm_close(theOutWccpConnection);
    }
}

/*          
 * Functions for handling the requests.
 */

/*          
 * Accept the UDP packet
 */
void
wccpHandleUdp(int sock, void *not_used)
{
    struct sockaddr_in from;
    socklen_t from_len;
    int len;

    debug(80, 6) ("wccpHandleUdp: Called.\n");

    commSetSelect(sock, COMM_SELECT_READ, wccpHandleUdp, NULL, 0);
    from_len = sizeof(struct sockaddr_in);
    memset(&from, '\0', from_len);
    memset(&wccp_i_see_you, '\0', sizeof(wccp_i_see_you));

    Counter.syscalls.sock.recvfroms++;

    len = recvfrom(sock,
	&wccp_i_see_you,
	WCCP_RESPONSE_SIZE,
	0,
	(struct sockaddr *) &from,
	&from_len);
    if (len < 0)
	return;
    if (Config.Wccp.router.s_addr != from.sin_addr.s_addr)
	return;
    if (ntohl(wccp_i_see_you.version) != WCCP_VERSION)
	return;
    if (ntohl(wccp_i_see_you.type) != WCCP_I_SEE_YOU)
	return;
    if (!change) {
	change = wccp_i_see_you.change;
	return;
    }
    if (change != wccp_i_see_you.change) {
	change = wccp_i_see_you.change;
	if (wccpLowestIP(wccp_i_see_you))
	    if (!eventFind(wccpAssignBuckets, NULL))
		eventAdd("wccpAssignBuckets", wccpAssignBuckets, NULL, 30.0, 1);
    }
}

int
wccpLowestIP()
{
    int loop;
    for (loop = 0; loop < ntohl(wccp_i_see_you.number); loop++) {
	if (wccp_i_see_you.wccp_cache_entry[loop].ip_addr < local_ip)
	    return (0);
    }
    return (1);
}

void
wccpHereIam(void *voidnotused)
{
    debug(80, 6) ("wccpHereIam: Called\n");

    wccp_here_i_am.id = wccp_i_see_you.id;
    send(theOutWccpConnection,
	&wccp_here_i_am,
	sizeof(wccp_here_i_am),
	0);

    eventAdd("wccpHereIam", wccpHereIam, NULL, 10.0, 1);
}

void
wccpAssignBuckets(void *voidnotused)
{
    struct wccp_assign_bucket_t wccp_assign_bucket;
    int number_buckets, loop_buckets, loop, number_caches, \
        bucket = 0, *caches, offset;
    char buckets[WCCP_BUCKETS];
    void *buf;

    debug(80, 6) ("wccpAssignBuckets: Called\n");
    memset(&wccp_assign_bucket, '\0', sizeof(wccp_assign_bucket));
    memset(buckets, 0xFF, WCCP_BUCKETS);

    number_caches = ntohl(wccp_i_see_you.number);
    if (number_caches > WCCP_ACTIVE_CACHES)
	number_caches = WCCP_ACTIVE_CACHES;
    caches = xmalloc(sizeof(int) * number_caches);

    number_buckets = WCCP_BUCKETS / number_caches;
    for (loop = 0; loop < number_caches; loop++) {
	caches[loop] = wccp_i_see_you.wccp_cache_entry[loop].ip_addr;
	for (loop_buckets = 0; loop_buckets < number_buckets; loop_buckets++) {
	    buckets[bucket++] = loop;
	}
    }
    offset = sizeof(wccp_assign_bucket);
    buf = xmalloc(offset + WCCP_BUCKETS + (sizeof(*caches) * number_caches));
    wccp_assign_bucket.type = htonl(WCCP_ASSIGN_BUCKET);
    wccp_assign_bucket.id = wccp_i_see_you.id;
    wccp_assign_bucket.number = wccp_i_see_you.number;

    memcpy(buf, &wccp_assign_bucket, offset);
    memcpy(buf + offset, caches, (sizeof(*caches) * number_caches));
    offset += (sizeof(*caches) * number_caches);
    memcpy(buf + offset, buckets, WCCP_BUCKETS);
    offset += WCCP_BUCKETS;
    send(theOutWccpConnection,
	buf,
	offset,
	0);
    change = 0;
    xfree(caches);
    xfree(buf);
}

#endif /* USE_WCCP */


/*
 * $Id$
 *
 * DEBUG: section 42    ICMP Pinger program
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

#if USE_ICMP

#include "pinger.h"

/* Native Windows port doesn't have netinet support, so we emulate it.
 * At this time, Cygwin lacks icmp support in its include files, so we need
 * to use the native Windows port definitions.
 */

#if !defined(_SQUID_WIN32_)

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define PINGER_TIMEOUT 10

static int socket_from_squid = 0;
static int socket_to_squid = 1;

#else /* _SQUID_WIN32_ */

#ifdef _SQUID_MSWIN_

#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <process.h>

#define PINGER_TIMEOUT 5

static SOCKET socket_to_squid = -1;
#define socket_from_squid socket_to_squid

#else /* _SQUID_MSWIN */

/* Cygwin */

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define PINGER_TIMEOUT 10

static int socket_from_squid = 0;
static int socket_to_squid = 1;

#endif /* _SQUID_MSWIN_ */

#endif /* _SQUID_WIN32_ */

#include "../libpinger/icmp_v4.h"
#include "../libpinger/icmp_v6.h"

typedef struct {
    struct timeval tv;
    unsigned char opcode;
    char payload[MAX_PAYLOAD];
} icmpEchoData;

static void pingerRecv(void);
static void pingerLog(int, struct in_addr, int, int);
static void pingerSendtoSquid(pingerReplyData * preply);
static void pingerOpen(void);
static void pingerClose(void);

#ifdef _SQUID_MSWIN_
void
Win32SockCleanup(void)
{
    WSACleanup();
    return;
}
#endif /* ifdef _SQUID_MSWIN_ */


struct pingerv4_state v4_state;

void
pingerOpen(void)
{
#ifdef _SQUID_MSWIN_
    WSADATA wsaData;
    WSAPROTOCOL_INFO wpi;
    char buf[sizeof(wpi)];
    int x;
    struct sockaddr_in PS;

    WSAStartup(2, &wsaData);
    atexit(Win32SockCleanup);

    getCurrentTime();
    _db_init("ALL,1");
    setmode(0, O_BINARY);
    setmode(1, O_BINARY);
    x = read(0, buf, sizeof(wpi));

    if (x < sizeof(wpi)) {
	getCurrentTime();
	debug(42, 0) ("pingerOpen: read: FD 0: %s\n", xstrerror());
	write(1, "ERR\n", 4);
	exit(1);
    }
    xmemcpy(&wpi, buf, sizeof(wpi));

    write(1, "OK\n", 3);
    x = read(0, buf, sizeof(PS));
    if (x < sizeof(PS)) {
	getCurrentTime();
	debug(42, 0) ("pingerOpen: read: FD 0: %s\n", xstrerror());
	write(1, "ERR\n", 4);
	exit(1);
    }
    xmemcpy(&PS, buf, sizeof(PS));
#endif

    debug(42, 0) ("pingerOpen: ICMP socket opened\n");
#ifdef _SQUID_MSWIN_
    socket_to_squid =
	WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
	&wpi, 0, 0);
    if (socket_to_squid == -1) {
	getCurrentTime();
	debug(42, 0) ("pingerOpen: WSASocket: %s\n", xstrerror());
	write(1, "ERR\n", 4);
	exit(1);
    }
    x = connect(socket_to_squid, (struct sockaddr *) &PS, sizeof(PS));
    if (SOCKET_ERROR == x) {
	getCurrentTime();
	debug(42, 0) ("pingerOpen: connect: %s\n", xstrerror());
	write(1, "ERR\n", 4);
	exit(1);
    }
    write(1, "OK\n", 3);
    memset(buf, 0, sizeof(buf));
    x = recv(socket_to_squid, buf, sizeof(buf), 0);
    if (x < 3) {
	debug(42, 0) ("pingerOpen: recv: %s\n", xstrerror());
	exit(1);
    }
    x = send(socket_to_squid, buf, strlen(buf), 0);
    if (x < 3 || strncmp("OK\n", buf, 3)) {
	debug(42, 0) ("pingerOpen: recv: %s\n", xstrerror());
	exit(1);
    }
    getCurrentTime();
    debug(42, 0) ("pingerOpen: Squid socket opened\n");
#endif
}

void
pingerClose(void)
{
    pingerv4_close_icmpsock(&v4_state);
#ifdef _SQUID_MSWIN_
    shutdown(socket_to_squid, SD_BOTH);
    close(socket_to_squid);
    socket_to_squid = -1;
#endif
}

static void
pingerSendEcho(struct in_addr to, int opcode, char *payload, int len)
{
    icmpEchoData echo;
    int icmp_pktsize;

    /* Assemble the icmpEcho payload */
    echo.opcode = (unsigned char) opcode;
    memcpy(&echo.tv, &current_time, sizeof(current_time));

    /* size of the IcmpEchoData header */
    icmp_pktsize = sizeof(struct timeval) + sizeof(char);

    /* If there's a payload, add it */
    if (payload) {
        memcpy(echo.payload, payload, MIN(len, MAX_PAYLOAD));
        icmp_pktsize += MIN(len, MAX_PAYLOAD);
    }

    pingerv4SendEcho(&v4_state, to, opcode, (char *) &echo, icmp_pktsize);
    pingerLog(ICMP_ECHO, to, 0, 0);
}

/*
 * This is an IPv4-specific function for now.
 */
static void
pingerRecv(void)
{
    char *pkt;
    struct timeval now;
    icmpEchoData *echo;
    static pingerReplyData preply;
    struct timeval tv;
    struct sockaddr_in *v4;

    int icmp_type, payload_len, hops;
    struct in_addr from;

    debug(42, 9) ("%s: called\n", __func__);

    pkt = pingerv4RecvEcho(&v4_state, &icmp_type, &payload_len,
      &from, &hops);
    debug(42, 9) ("%s: returned %p\n", __func__, pkt);

    if (pkt == NULL)
        return;

#if GETTIMEOFDAY_NO_TZP
    gettimeofday(&now);
#else
    gettimeofday(&now, NULL);
#endif

    debug(42, 9) ("pingerRecv: %d payload bytes from %s\n", payload_len,
      inet_ntoa(from));
    echo = (icmpEchoData *) pkt;

    /* Set V4 address in preply */
    v4 = (struct sockaddr_in *) &preply.from;
    v4->sin_family = AF_INET;
    v4->sin_port = 0;
    v4->sin_addr = from;

    preply.opcode = echo->opcode;
    preply.hops = hops;
    memcpy(&tv, &echo->tv, sizeof(tv));
    preply.rtt = tvSubMsec(tv, now);
    preply.psize = payload_len;
    pingerSendtoSquid(&preply);
    pingerLog(icmp_type, from, preply.rtt, preply.hops);
}

static void
pingerLog(int icmp_type, struct in_addr addr, int rtt, int hops)
{
    debug(42, 2) ("pingerLog: %9d.%06d %-16s %d %-15.15s %dms %d hops\n",
	(int) current_time.tv_sec,
	(int) current_time.tv_usec,
	inet_ntoa(addr),
	(int) icmp_type,
	icmpPktStr[icmp_type],
	rtt,
	hops);
}

static int
pingerReadRequest(void)
{
    static pingerEchoData pecho;
    int n;
    int guess_size;
    struct sockaddr_in *v4;

    memset(&pecho, '\0', sizeof(pecho));
    n = read(socket_from_squid, (char *) &pecho, sizeof(pecho));
    if (n < 0) {
        debug(42, 0) ("pingerReadRequest: socket %d: read() failed; errno %d\n", socket_from_squid, errno);
	return n;
    }
    if (0 == n) {
	/* EOF indicator */
	fprintf(stderr, "EOF encountered\n");
	errno = 0;
	return -1;
    }
    guess_size = n - (sizeof(pingerEchoData) - PINGER_PAYLOAD_SZ);
    if (guess_size != pecho.psize) {
	fprintf(stderr, "size mismatch, guess=%d psize=%d\n",
	    guess_size, pecho.psize);
	/* don't process this message, but keep running */
	return 0;
    }

#warning IPv6-ify this!
    v4 = (struct sockaddr_in *) &pecho.to;
    pingerSendEcho(v4->sin_addr,
	pecho.opcode,
	pecho.payload,
	pecho.psize);
    return n;
}

static void
pingerSendtoSquid(pingerReplyData * preply)
{
    int len = sizeof(pingerReplyData) - MAX_PKT_SZ + preply->psize;
    if (send(socket_to_squid, (char *) preply, len, 0) < 0) {
	debug(42, 0) ("pingerSendtoSquid: send: %s\n", xstrerror());
	pingerClose();
	exit(1);
    }
}

int
main(int argc, char *argv[])
{
    fd_set R;
    int x;
    struct timeval tv;
    const char *debug_args = "ALL,1";
    char *t;
    time_t last_check_time = 0;

/*
 * cevans - do this first. It grabs a raw socket. After this we can
 * drop privs
 */
    if ((t = getenv("SQUID_DEBUG")))
	debug_args = xstrdup(t);
    getCurrentTime();
    _db_init(debug_args);
    _db_set_stderr_debug(1);

    /* Open the IPC sockets */
    pingerOpen();

    /* Setup the IPv4 ICMP socket */
    pingerv4_state_init(&v4_state, getpid() & 0xffff);
    if (! pingerv4_open_icmpsock(&v4_state))
        exit(1);

    setgid(getgid());
    setuid(getuid());

    for (;;) {
	tv.tv_sec = PINGER_TIMEOUT;
	tv.tv_usec = 0;
	FD_ZERO(&R);
	FD_SET(socket_from_squid, &R);
	FD_SET(v4_state.icmp_sock, &R);
	x = select(v4_state.icmp_sock + 1, &R, NULL, NULL, &tv);
	getCurrentTime();
	if (x < 0 && errno == EINTR)
		continue;
	if (x < 0) {
	    pingerClose();
	    exit(1);
	}
	if (FD_ISSET(socket_from_squid, &R))
	    if (pingerReadRequest() < 0) {
		debug(42, 0) ("Pinger exiting.\n");
		pingerClose();
		exit(1);
	    }
	if (FD_ISSET(v4_state.icmp_sock, &R))
	    pingerRecv();
	if (PINGER_TIMEOUT + last_check_time < squid_curtime) {
	    debug(42, 2) ("pinger: timeout occured\n");
	    if (send(socket_to_squid, (char *) &tv, 0, 0) < 0) {
		debug(42, 0) ("Pinger: send socket_to_squid failed\n");
		pingerClose();
		exit(1);
	    } else {
		debug(42, 2) ("Pinger: send socket_to_squid OK\n");
	    }
	    last_check_time = squid_curtime;
	}
    }
    /* NOTREACHED */
}

#else
#include <stdio.h>
int
main(int argc, char *argv[])
{
    fprintf(stderr, "%s: ICMP support not compiled in.\n", argv[0]);
    return 1;
}
#endif /* USE_ICMP */

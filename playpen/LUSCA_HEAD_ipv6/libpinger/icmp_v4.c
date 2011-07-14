/*
 * $Id$
 *
 * DEBUG: section 42    ICMP/Pinger
 * AUTHOR: Harvest Derived
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

#include "../include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#if !defined(_SQUID_WIN32_)

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define PINGER_TIMEOUT 10

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

#include "include/util.h"
#include "libcore/tools.h"
#include "libsqdebug/debug.h"

#include "icmp_v4.h"

/* IPv4 ICMP type strings */

const char *icmpPktStr[] =
{
    "Echo Reply",
    "ICMP 1",
    "ICMP 2",
    "Destination Unreachable",
    "Source Quench",
    "Redirect",
    "ICMP 6",
    "ICMP 7",
    "Echo",
    "ICMP 9",
    "ICMP 10",
    "Time Exceeded",
    "Parameter Problem",
    "Timestamp",
    "Timestamp Reply",
    "Info Request",
    "Info Reply",
    "Out of Range Type"
};

int icmp_ident = -1;
int icmp_pkts_sent = 0;

static int
in_cksum(unsigned short *ptr, int size)
{
    long sum;
    unsigned short oddbyte;
    unsigned short answer;
    sum = 0;
    while (size > 1) {
        sum += *ptr++;
        size -= 2;
    }
    if (size == 1) {
        oddbyte = 0;
        *((unsigned char *) &oddbyte) = *(unsigned char *) ptr;
        sum += oddbyte;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = (unsigned short) ~sum;
    return (answer);
}

/*
 * Assemble an IPv4 ICMP Echo packet.
 *
 * This doesn't know about the payload layout; it simply assembles the
 * packet and fires it off.
 */
void
pingerv4SendEcho(int icmp_sock, struct in_addr to, int opcode, char *payload,
  int len)
{
    LOCAL_ARRAY(char, pkt, MAX_PKT_SZ);
    struct icmphdr *icmp = NULL;

    int icmp_pktsize = sizeof(struct icmphdr);
    struct sockaddr_in S;
    memset(pkt, '\0', MAX_PKT_SZ);
    icmp = (struct icmphdr *) (void *) pkt;

    /*
     * cevans - beware signed/unsigned issues in untrusted data from
     * the network!!
     */
    if (len < 0) {
        len = 0;
    }
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_cksum = 0;
    icmp->icmp_id = icmp_ident;
    icmp->icmp_seq = (u_short) icmp_pkts_sent++;

    /* The ICMP payload is the entire 'payload' + 'len' */
    /* The caller will setup the tmimestamp and icmpEchoData */

    if (payload) {
        if (len > MAX_PAYLOAD)
            len = MAX_PAYLOAD;
        xmemcpy(pkt + sizeof(struct icmphdr), payload, len);
        icmp_pktsize += len;
    }

    icmp->icmp_cksum = in_cksum((u_short *) icmp, icmp_pktsize);
    S.sin_family = AF_INET;
    /*
     * cevans: alert: trusting to-host, was supplied in network packet
     */
    S.sin_addr = to;
    S.sin_port = 0;
    assert(icmp_pktsize <= MAX_PKT_SZ);
    sendto(icmp_sock,
      pkt,
      icmp_pktsize,
      0,
      (struct sockaddr *) &S,
      sizeof(struct sockaddr_in));
}


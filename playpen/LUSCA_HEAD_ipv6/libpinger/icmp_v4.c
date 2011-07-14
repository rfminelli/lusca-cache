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
#include <arpa/inet.h>

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

static int
ipHops(int ttl)
{
    if (ttl < 33)
        return 33 - ttl;
    if (ttl < 63)
        return 63 - ttl;        /* 62 = (64+60)/2 */
    if (ttl < 65)
        return 65 - ttl;        /* 62 = (64+60)/2 */
    if (ttl < 129)
        return 129 - ttl;
    if (ttl < 193)
        return 193 - ttl;
    return 256 - ttl;
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

/*
 * Receive an ICMP packet.
 *
 * If NULL is returned, the packet isn't valid or is the wrong type.
 *
 * If non-NULL is returned, the pointer refers to the beginning of
 * the ICMP response payload; *len is set to the payload length.
 */
char *
pingerv4RecvEcho(int icmp_sock, int *icmp_type, int *payload_len,
  struct in_addr *src, int *hops)
{
    int n;
    socklen_t fromlen;
    struct sockaddr_in from;
    int iphdrlen = 20;
    struct iphdr *ip = NULL;
    struct icmphdr *icmp = NULL;
    static char *pkt = NULL;

    (*icmp_type = -1);

    if (pkt == NULL)
        pkt = xmalloc(MAX_PKT_SZ);
    fromlen = sizeof(from);

    n = recvfrom(icmp_sock,
      pkt,
      MAX_PKT_SZ,
      0,
      (struct sockaddr *) &from,
      &fromlen);

    debug(42, 9) ("pingerRecv: %d bytes from %s\n", n,
      inet_ntoa(from.sin_addr));
    ip = (struct iphdr *) (void *) pkt;
    (*src) = from.sin_addr;

#if HAVE_IP_HL
    iphdrlen = ip->ip_hl << 2;
#else /* HAVE_IP_HL */
#if WORDS_BIGENDIAN
    iphdrlen = (ip->ip_vhl >> 4) << 2;
#else
    iphdrlen = (ip->ip_vhl & 0xF) << 2;
#endif
#endif /* HAVE_IP_HL */
    icmp = (struct icmphdr *) (void *) (pkt + iphdrlen);

    if (icmp->icmp_type != ICMP_ECHOREPLY) {
        debug(42, 9) ("%s: icmp_type=%d\n", __func__, icmp->icmp_type);
        return NULL;
    }
    if (icmp->icmp_id != icmp_ident) {
        debug(42, 9) ("%s: icmp_id=%d, ident should be %d\n", __func__,
          icmp->icmp_id, icmp_ident);
        return NULL;
    }

    /* record the ICMP results */
    (*hops) = ipHops(ip->ip_ttl);
    (*payload_len) = n - iphdrlen - sizeof(struct icmphdr);
    if (*payload_len < 0)
        return NULL;

    debug(42, 9) ("%s: hops=%d, len=%d\n", __func__, ipHops(ip->ip_ttl),
      n - iphdrlen);

    /* There may be no payload; the caller should check len first */
    return (pkt + iphdrlen + sizeof(struct icmphdr));
}

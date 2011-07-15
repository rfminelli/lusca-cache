#include "../include/config.h"

/* This needs to be tested on Windows! */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * Some system headers are only neeed internally here.
 * They should not be included via the header.
 */
#if HAVE_NETINET_IP6_H
#include <netinet/ip6.h>
#endif

#include <netinet/icmp6.h>

#include "include/util.h"
#include "libcore/varargs.h"
#include "libsqdebug/debug.h"
#include "libsqinet/sqinet.h"

#include "icmp_v6.h"

#define MAX_PKT6_SZ    (MAX_PAYLOAD + sizeof(struct timeval) + sizeof (char) + \
                         sizeof(struct icmp6_hdr) + 1)

// Icmp6 OP-Codes
// see http://www.iana.org/assignments/icmpv6-parameters
// NP: LowPktStr is for codes 0-127
const char *icmp6LowPktStr[] = {
    "ICMP 0",                   // 0
    "Destination Unreachable",  // 1 - RFC2463
    "Packet Too Big",           // 2 - RFC2463
    "Time Exceeded",            // 3 - RFC2463
    "Parameter Problem",        // 4 - RFC2463
    "ICMP 5",                   // 5
    "ICMP 6",                   // 6
    "ICMP 7",                   // 7
    "ICMP 8",                   // 8
    "ICMP 9",                   // 9
    "ICMP 10"                   // 10
};

// NP: HighPktStr is for codes 128-255
const char *icmp6HighPktStr[] = {
    "Echo Request",                             // 128 - RFC2463
    "Echo Reply",                               // 129 - RFC2463
    "Multicast Listener Query",                 // 130 - RFC2710
    "Multicast Listener Report",                // 131 - RFC2710
    "Multicast Listener Done",                  // 132 - RFC2710
    "Router Solicitation",                      // 133 - RFC4861
    "Router Advertisement",                     // 134 - RFC4861
    "Neighbor Solicitation",                    // 135 - RFC4861
    "Neighbor Advertisement",                   // 136 - RFC4861
    "Redirect Message",                         // 137 - RFC4861
    "Router Renumbering",                       // 138 - Crawford
    "ICMP Node Information Query",              // 139 - RFC4620
    "ICMP Node Information Response",           // 140 - RFC4620
    "Inverse Neighbor Discovery Solicitation",  // 141 - RFC3122
    "Inverse Neighbor Discovery Advertisement", // 142 - RFC3122
    "Version 2 Multicast Listener Report",      // 143 - RFC3810
    "Home Agent Address Discovery Request",     // 144 - RFC3775
    "Home Agent Address Discovery Reply",       // 145 - RFC3775
    "Mobile Prefix Solicitation",               // 146 - RFC3775
    "Mobile Prefix Advertisement",              // 147 - RFC3775
    "Certification Path Solicitation",          // 148 - RFC3971
    "Certification Path Advertisement",         // 149 - RFC3971
    "ICMP Experimental (150)",                  // 150 - RFC4065
    "Multicast Router Advertisement",           // 151 - RFC4286
    "Multicast Router Solicitation",            // 152 - RFC4286
    "Multicast Router Termination",             // 153 - [RFC4286]
    "ICMP 154",
    "ICMP 155",
    "ICMP 156",
    "ICMP 157",
    "ICMP 158",
    "ICMP 159",
    "ICMP 160"
};


/*
 * XXX Duplicated with icmp_v4.c
 */
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

void
pingerv6SendEcho(struct pingerv6_state *state, sqaddr_t *to,
  int opcode, char *payload, int len)
{
    char pkt[MAX_PKT6_SZ];
    struct icmp6_hdr *icmp = NULL;
    size_t icmp6_pktsize = 0;
    int x;

    /*
     * cevans - beware signed/unsigned issues in untrusted data from
     * the network!!
     */
    if (len < 0)
        len = 0;

    /* Construct Icmp6 ECHO header */
    memset(pkt, 0, MAX_PKT6_SZ);
    icmp = (struct icmp6_hdr *) pkt;
    icmp->icmp6_type = ICMP6_ECHO_REQUEST;
    icmp->icmp6_code = 0;
    icmp->icmp6_cksum = 0;
    icmp->icmp6_id = state->icmp_ident;
    icmp->icmp6_seq = (u_short) state->icmp_pkts_sent++;

    icmp6_pktsize = sizeof(struct icmp6_hdr);

    /* Add payload */
    if (len > 0) {
        memcpy(pkt + icmp6_pktsize, payload,
          MIN(len, MAX_PKT6_SZ - icmp6_pktsize));
       icmp6_pktsize = MIN(icmp6_pktsize + len, MAX_PKT6_SZ);
    }

    /* Calculate checksum */
    icmp->icmp6_cksum = in_cksum((u_short *) pkt, icmp6_pktsize);

    /* Send! */
    x = sendto(state->icmp_sock, (const void *) pkt,
      icmp6_pktsize, 0, sqinet_get_entry_ro(to),
      sqinet_get_length(to));

    if (x < 0)
         debug(42, 0) ("%s: sendto(icmpsock): %s\n", __func__, xstrerror());
}

char *
pingerv6RecvEcho(struct pingerv6_state *state, int *icmp_type,
  int *payload_len, sqaddr_t *src, int *hops)
{
    int n;
    struct sockaddr_storage from;
    static char *pkt = NULL;
    struct icmp6_hdr *icmp6header = NULL;
    socklen_t fromlen;

    (*icmp_type) = -1;
    fromlen = sizeof(from);

    if (state->icmp_sock < 0) {
        debug(42, 0) ("dropping ICMPv6 read. No socket!?\n");
        return NULL;
    }

    if (pkt == NULL) {
        pkt = (char *)xmalloc(MAX_PKT6_SZ);
    }

    n = recvfrom(state->icmp_sock,
                 (void *)pkt,
                 MAX_PKT6_SZ,
                 0,
                 (struct sockaddr *) &from,
                 &fromlen);

    sqinet_set_sockaddr(src, &from);

    icmp6header = (struct icmp6_hdr *) pkt;

    if (icmp6header->icmp6_type != ICMP6_ECHO_REPLY) {
        debug(42, 1) ("%s: unknown ICMP response, code %d\n",
          __func__, icmp6header->icmp6_type);
        return NULL;
    }
    (*icmp_type) = icmp6header->icmp6_type;

    if (icmp6header->icmp6_id != state->icmp_ident) {
        debug(42, 1) ("%s: unknown ICMP id: %d != %d\n",
          __func__, icmp6header->icmp6_id,
          state->icmp_ident);
        return NULL;
    }

    (*hops) = 1;    /* XXX there's no easy way to extract hops from the reply */

    (*payload_len) = n - sizeof(struct icmp6_hdr);
    return pkt + sizeof(struct icmp6_hdr);
}

void
pingerv6_state_init(struct pingerv6_state *state, int icmp_ident)
{
    state->icmp_sock = -1;
    state->icmp_pkts_sent = 0;
    state->icmp_ident = icmp_ident;
}

int
pingerv6_open_icmpsock(struct pingerv6_state *state)
{
    state->icmp_sock = socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (state->icmp_sock < 0) {
        debug(42, 0) ("%s: icmp_sock: %s\n", __func__, xstrerror());
        return 0;
    }
    return 1;
}

void
pingerv6_close_icmpsock(struct pingerv6_state *state)
{
    close(state->icmp_sock);
    state->icmp_sock = -1;
}


/*
 * $Id$
 *
 * DEBUG: section 15    Neighbor Routines
 * AUTHOR: Harvest Derived
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

/*
 * Copyright (c) 1994, 1995.  All rights reserved.
 *  
 *   The Harvest software was developed by the Internet Research Task
 *   Force Research Group on Resource Discovery (IRTF-RD):
 *  
 *         Mic Bowman of Transarc Corporation.
 *         Peter Danzig of the University of Southern California.
 *         Darren R. Hardy of the University of Colorado at Boulder.
 *         Udi Manber of the University of Arizona.
 *         Michael F. Schwartz of the University of Colorado at Boulder.
 *         Duane Wessels of the University of Colorado at Boulder.
 *  
 *   This copyright notice applies to software in the Harvest
 *   ``src/'' directory only.  Users should consult the individual
 *   copyright notices in the ``components/'' subdirectories for
 *   copyright information about other software bundled with the
 *   Harvest source code distribution.
 *  
 * TERMS OF USE
 *   
 *   The Harvest software may be used and re-distributed without
 *   charge, provided that the software origin and research team are
 *   cited in any use of the system.  Most commonly this is
 *   accomplished by including a link to the Harvest Home Page
 *   (http://harvest.cs.colorado.edu/) from the query page of any
 *   Broker you deploy, as well as in the query result pages.  These
 *   links are generated automatically by the standard Broker
 *   software distribution.
 *   
 *   The Harvest software is provided ``as is'', without express or
 *   implied warranty, and with no support nor obligation to assist
 *   in its use, correction, modification or enhancement.  We assume
 *   no liability with respect to the infringement of copyrights,
 *   trade secrets, or any patents, and are not responsible for
 *   consequential damages.  Proper use of the Harvest software is
 *   entirely the responsibility of the user.
 *  
 * DERIVATIVE WORKS
 *  
 *   Users may make derivative works from the Harvest software, subject 
 *   to the following constraints:
 *  
 *     - You must include the above copyright notice and these 
 *       accompanying paragraphs in all forms of derivative works, 
 *       and any documentation and other materials related to such 
 *       distribution and use acknowledge that the software was 
 *       developed at the above institutions.
 *  
 *     - You must notify IRTF-RD regarding your distribution of 
 *       the derivative work.
 *  
 *     - You must clearly notify users that your are distributing 
 *       a modified version and not the original Harvest software.
 *  
 *     - Any derivative product is also subject to these copyright 
 *       and use restrictions.
 *  
 *   Note that the Harvest software is NOT in the public domain.  We
 *   retain copyright, as specified above.
 *  
 * HISTORY OF FREE SOFTWARE STATUS
 *  
 *   Originally we required sites to license the software in cases
 *   where they were going to build commercial products/services
 *   around Harvest.  In June 1995 we changed this policy.  We now
 *   allow people to use the core Harvest software (the code found in
 *   the Harvest ``src/'' directory) for free.  We made this change
 *   in the interest of encouraging the widest possible deployment of
 *   the technology.  The Harvest software is really a reference
 *   implementation of a set of protocols and formats, some of which
 *   we intend to standardize.  We encourage commercial
 *   re-implementations of code complying to this set of standards.  
 */

#include "squid.h"

/* count mcast group peers every 15 minutes */
#define MCAST_COUNT_RATE 900

static int peerAllowedToUse(const peer *, request_t *);
static int peerWouldBePinged(const peer *, request_t *);
static void neighborRemove(peer *);
static peer *whichPeer(const struct sockaddr_in *from);
static void neighborAlive(peer *, const MemObject *, const icp_common_t *);
static void neighborCountIgnored(peer *, icp_opcode op_unused);
static void peerRefreshDNS(void *);
static IPH peerDNSConfigure;
static EVH peerCheckConnect;
static IPH peerCheckConnect2;
static CNCB peerCheckConnectDone;
static void peerCountMcastPeersDone(void *data);
static void peerCountMcastPeersStart(void *data);
static void peerCountMcastPeersSchedule(peer * p, time_t when);
static IRCB peerCountHandleIcpReply;
static void neighborIgnoreNonPeer(const struct sockaddr_in *, icp_opcode);
static OBJH neighborDumpPeers;
static OBJH neighborDumpNonPeers;
static void dump_peers(StoreEntry * sentry, peer * peers);

static icp_common_t echo_hdr;
static u_short echo_port;

static int NLateReplies = 0;
static peer *first_ping = NULL;

char *
neighborTypeStr(const peer * p)
{
    if (p->type == PEER_NONE)
	return "Non-Peer";
    if (p->type == PEER_SIBLING)
	return "Sibling";
    if (p->type == PEER_MULTICAST)
	return "Multicast Group";
    return "Parent";
}


static peer *
whichPeer(const struct sockaddr_in *from)
{
    int j;
    u_short port = ntohs(from->sin_port);
    struct in_addr ip = from->sin_addr;
    peer *p = NULL;
    debug(15, 3) ("whichPeer: from %s port %d\n", inet_ntoa(ip), port);
    for (p = Config.peers; p; p = p->next) {
	for (j = 0; j < p->n_addresses; j++) {
	    if (ip.s_addr == p->addresses[j].s_addr && port == p->icp_port) {
		return p;
	    }
	}
    }
    return NULL;
}

static peer_t
neighborType(const peer * p, const request_t * request)
{
    const struct _domain_type *d = NULL;
    for (d = p->typelist; d; d = d->next) {
	if (matchDomainName(d->domain, request->host))
	    if (d->type != PEER_NONE)
		return d->type;
    }
    return p->type;
}

/*
 * peerAllowedToUse
 *
 * this function figures out if it is appropriate to fetch REQUEST
 * from PEER.
 */
static int
peerAllowedToUse(const peer * p, request_t * request)
{
    const struct _domain_ping *d = NULL;
    int do_ping = 1;
    const struct _acl_list *a = NULL;
    aclCheck_t checklist;
    assert(request != NULL);
    if (EBIT_TEST(request->flags, REQ_NOCACHE))
	if (neighborType(p, request) == PEER_SIBLING)
	    return 0;
    if (EBIT_TEST(request->flags, REQ_REFRESH))
	if (neighborType(p, request) == PEER_SIBLING)
	    return 0;
    if (p->pinglist == NULL && p->acls == NULL)
	return do_ping;
    do_ping = 0;
    for (d = p->pinglist; d; d = d->next) {
	if (matchDomainName(d->domain, request->host)) {
	    do_ping = d->do_ping;
	    break;
	}
	do_ping = !d->do_ping;
    }
    if (0 == do_ping)
	return do_ping;
    checklist.src_addr = request->client_addr;
    checklist.request = request;
    for (a = p->acls; a; a = a->next) {
	if (aclMatchAcl(a->acl, &checklist)) {
	    do_ping = a->op;
	    break;
	}
	do_ping = !a->op;
    }
    return do_ping;
}

/* Return TRUE if it is okay to send an ICP request to this peer.   */
static int
peerWouldBePinged(const peer * p, request_t * request)
{
    if (!peerAllowedToUse(p, request))
	return 0;
    if (EBIT_TEST(p->options, NEIGHBOR_NO_QUERY))
	return 0;
    if (EBIT_TEST(p->options, NEIGHBOR_MCAST_RESPONDER))
	return 0;
    /* the case below seems strange, but can happen if the
     * URL host is on the other side of a firewall */
    if (p->type == PEER_SIBLING)
	if (!EBIT_TEST(request->flags, REQ_HIERARCHICAL))
	    return 0;
    if (p->icp_port == echo_port)
	if (!neighborUp(p))
	    return 0;
    if (p->n_addresses == 0)
	return 0;
    return 1;
}

/* Return TRUE if it is okay to send an HTTP request to this peer. */
int
peerHTTPOkay(const peer * p, request_t * request)
{
    if (!peerAllowedToUse(p, request))
	return 0;
    if (!neighborUp(p))
	return 0;
    return 1;
}

int
neighborsCount(request_t * request)
{
    peer *p = NULL;
    int count = 0;
    for (p = Config.peers; p; p = p->next)
	if (peerWouldBePinged(p, request))
	    count++;
    debug(15, 3) ("neighborsCount: %d\n", count);
    return count;
}

peer *
getSingleParent(request_t * request)
{
    peer *p = NULL;
    peer *q = NULL;
    for (q = Config.peers; q; q = q->next) {
	if (!peerHTTPOkay(q, request))
	    continue;
	if (neighborType(q, request) != PEER_PARENT)
	    return NULL;	/* oops, found SIBLING */
	if (p)
	    return NULL;	/* oops, found second parent */
	p = q;
    }
    if (p != NULL && !EBIT_TEST(p->options, NEIGHBOR_NO_QUERY))
	return NULL;
    debug(15, 3) ("getSingleParent: returning %s\n", p ? p->host : "NULL");
    return p;
}

peer *
getFirstUpParent(request_t * request)
{
    peer *p = NULL;
    for (p = Config.peers; p; p = p->next) {
	if (!neighborUp(p))
	    continue;
	if (neighborType(p, request) != PEER_PARENT)
	    continue;
	if (!peerHTTPOkay(p, request))
	    continue;
	break;
    }
    debug(15, 3) ("getFirstUpParent: returning %s\n", p ? p->host : "NULL");
    return p;
}

peer *
getRoundRobinParent(request_t * request)
{
    peer *p;
    peer *q = NULL;
    for (p = Config.peers; p; p = p->next) {
	if (!EBIT_TEST(p->options, NEIGHBOR_ROUNDROBIN))
	    continue;
	if (neighborType(p, request) != PEER_PARENT)
	    continue;
	if (!peerHTTPOkay(p, request))
	    continue;
	if (q && q->rr_count < p->rr_count)
	    continue;
	q = p;
    }
    if (q)
	q->rr_count++;
    debug(15, 3) ("getRoundRobinParent: returning %s\n", q ? q->host : "NULL");
    return q;
}

peer *
getDefaultParent(request_t * request)
{
    peer *p = NULL;
    for (p = Config.peers; p; p = p->next) {
	if (neighborType(p, request) != PEER_PARENT)
	    continue;
	if (!EBIT_TEST(p->options, NEIGHBOR_DEFAULT_PARENT))
	    continue;
	if (!peerHTTPOkay(p, request))
	    continue;
	debug(15, 3) ("getDefaultParent: returning %s\n", p->host);
	return p;
    }
    return NULL;
}

peer *
getNextPeer(peer * p)
{
    return p->next;
}

peer *
getFirstPeer(void)
{
    return Config.peers;
}

static void
neighborRemove(peer * target)
{
    peer *p = NULL;
    peer **P = NULL;
    p = Config.peers;
    P = &Config.peers;
    while (p) {
	if (target == p)
	    break;
	P = &p->next;
	p = p->next;
    }
    if (p) {
	*P = p->next;
	peerDestroy(p);
	Config.npeers--;
    }
    first_ping = Config.peers;
}

void
neighbors_open(int fd)
{
    struct sockaddr_in name;
    int len = sizeof(struct sockaddr_in);
    struct servent *sep = NULL;
    memset(&name, '\0', sizeof(struct sockaddr_in));
    if (getsockname(fd, (struct sockaddr *) &name, &len) < 0)
	debug(15, 1) ("getsockname(%d,%p,%p) failed.\n", fd, &name, &len);
    peerRefreshDNS(NULL);
    if (0 == echo_hdr.opcode) {
	echo_hdr.opcode = ICP_SECHO;
	echo_hdr.version = ICP_VERSION_CURRENT;
	echo_hdr.length = 0;
	echo_hdr.reqnum = 0;
	echo_hdr.flags = 0;
	echo_hdr.pad = 0;
	echo_hdr.shostid = name.sin_addr.s_addr;
	sep = getservbyname("echo", "udp");
	echo_port = sep ? ntohs((u_short) sep->s_port) : 7;
    }
    first_ping = Config.peers;
    cachemgrRegister("server_list",
	"Peer Cache Statistics",
	neighborDumpPeers, 0);
    cachemgrRegister("non_peers",
	"List of Unknown sites sending ICP messages",
	neighborDumpNonPeers, 0);
}

int
neighborsUdpPing(request_t * request,
    StoreEntry * entry,
    IRCB * callback,
    void *callback_data,
    int *exprep,
    double *exprtt)
{
    const char *url = storeUrl(entry);
    MemObject *mem = entry->mem_obj;
    peer *p = NULL;
    int i;
    int reqnum = 0;
    int flags;
    icp_common_t *query;
    int queries_sent = 0;
    int peers_pinged = 0;

    if (Config.peers == NULL)
	return 0;
    if (theOutIcpConnection < 0)
	fatal("neighborsUdpPing: There is no ICP socket!");
    assert(entry->swap_status == SWAPOUT_NONE);
    mem->start_ping = current_time;
    mem->icp_reply_callback = callback;
    mem->ircb_data = callback_data;
    *exprtt = 0.0;
    for (i = 0, p = first_ping; i++ < Config.npeers; p = p->next) {
	if (p == NULL)
	    p = Config.peers;
	debug(15, 5) ("neighborsUdpPing: Peer %s\n", p->host);
	if (!peerWouldBePinged(p, request))
	    continue;		/* next peer */
	peers_pinged++;
	debug(15, 4) ("neighborsUdpPing: pinging peer %s for '%s'\n",
	    p->host, url);
	if (p->type == PEER_MULTICAST)
	    mcastSetTtl(theOutIcpConnection, p->mcast.ttl);
	reqnum = mem->reqnum;
	debug(15, 3) ("neighborsUdpPing: key = '%s'\n", storeKeyText(entry->key));
	debug(15, 3) ("neighborsUdpPing: reqnum = %d\n", reqnum);

#if USE_HTCP
	if (EBIT_TEST(p->options, NEIGHBOR_HTCP)) {
	    debug(15, 0) ("neighborsUdpPing: sending HTCP query\n");
	    htcpQuery(entry, request, p);
	} else
#endif
	if (p->icp_port == echo_port) {
	    debug(15, 4) ("neighborsUdpPing: Looks like a dumb cache, send DECHO ping\n");
	    echo_hdr.reqnum = reqnum;
	    query = icpCreateMessage(ICP_DECHO, 0, url, reqnum, 0);
	    icpUdpSend(theOutIcpConnection,
		&p->in_addr,
		query,
		LOG_ICP_QUERY,
		0);
	} else {
	    flags = 0;
	    if (Config.onoff.query_icmp)
		if (p->icp_version == ICP_VERSION_2)
		    flags |= ICP_FLAG_SRC_RTT;
	    query = icpCreateMessage(ICP_QUERY, flags, url, reqnum, 0);
	    icpUdpSend(theOutIcpConnection,
		&p->in_addr,
		query,
		LOG_ICP_QUERY,
		0);
	}
	queries_sent++;

	p->stats.pings_sent++;
	if (p->type == PEER_MULTICAST) {
	    /*
	     * set a bogus last_reply time so neighborUp() never
	     * says a multicast peer is dead.
	     */
	    p->stats.last_reply = squid_curtime;
	    (*exprep) += p->mcast.n_replies_expected;
	} else if (squid_curtime - p->stats.last_query > Config.Timeout.deadPeer) {
	    /*
	     * fake a recent reply if its been a long time since our
	     * last query
	     */
	    p->stats.last_reply = squid_curtime;
	} else if (neighborUp(p)) {
	    /* its alive, expect a reply from it */
	    (*exprep)++;
	    (*exprtt) += (double) p->stats.rtt;
	} else {
	    /* Neighbor is dead; ping it anyway, but don't expect a reply */
	    /* log it once at the threshold */
	    if (p->stats.logged_state == PEER_ALIVE) {
		debug(15, 1) ("Detected DEAD %s: %s/%d/%d\n",
		    neighborTypeStr(p),
		    p->host, p->http_port, p->icp_port);
		p->stats.logged_state = PEER_DEAD;
	    }
	}
	p->stats.last_query = squid_curtime;
    }
    if ((first_ping = first_ping->next) == NULL)
	first_ping = Config.peers;

#if ALLOW_SOURCE_PING
    /* only do source_ping if we have neighbors */
    if (Config.npeers) {
	const ipcache_addrs *ia = NULL;
	struct sockaddr_in to_addr;
	char *host = request->host;
	if (!Config.onoff.source_ping) {
	    debug(15, 6) ("neighborsUdpPing: Source Ping is disabled.\n");
	} else if ((ia = ipcache_gethostbyname(host, 0))) {
	    debug(15, 6) ("neighborsUdpPing: Source Ping: to %s for '%s'\n",
		host, url);
	    echo_hdr.reqnum = reqnum;
	    if (icmp_sock != -1) {
		icmpSourcePing(ia->in_addrs[ia->cur], &echo_hdr, url);
	    } else {
		to_addr.sin_family = AF_INET;
		to_addr.sin_addr = ia->in_addrs[ia->cur];
		to_addr.sin_port = htons(echo_port);
		query = icpCreateMessage(ICP_SECHO, 0, url, reqnum, 0);
		icpUdpSend(theOutIcpConnection,
		    &to_addr,
		    query,
		    LOG_ICP_QUERY,
		    0);
	    }
	} else {
	    debug(15, 6) ("neighborsUdpPing: Source Ping: unknown host: %s\n",
		host);
	}
    }
#endif
#if LOG_ICP_NUMBERS
    request->hierarchy.n_sent = peers_pinged;
    request->hierarchy.n_expect = *exprep;
#endif
    /*
     * Average out the expected RTT and then double it
     */
    if (*exprep > 0)
	(*exprtt) = 2.0 * (*exprtt) / (double) (*exprep);
    else
	*exprtt = Config.neighborTimeout;
    return peers_pinged;
}

/* lookup the digest of a given peer */
lookup_t
peerDigestLookup(peer * p, request_t * request, StoreEntry * entry)
{
#if USE_CACHE_DIGESTS
    const cache_key *key = request ? storeKeyPublic(storeUrl(entry), request->method) : NULL;
    assert(p);
    assert(request);
    debug(15, 5) ("peerDigestLookup: peer %s\n", p->host);
    /* does the peeer have a valid digest? */
    if (EBIT_TEST(p->digest.flags, PD_DISABLED)) {
	debug(15, 5) ("peerDigestLookup: Disabled!\n");
	return LOOKUP_NONE;
    } else if (!peerAllowedToUse(p, request)) {
	debug(15, 5) ("peerDigestLookup: !peerAllowedToUse()\n");
	return LOOKUP_NONE;
    } else if (EBIT_TEST(p->digest.flags, PD_USABLE)) {
	debug(15, 5) ("peerDigestLookup: Usable!\n");
	/* fall through; put here to have common case on top */ ;
    } else if (!EBIT_TEST(p->digest.flags, PD_INITED)) {
	debug(15, 5) ("peerDigestLookup: !initialized\n");
	if (!EBIT_TEST(p->digest.flags, PD_INIT_PENDING)) {
	    EBIT_SET(p->digest.flags, PD_INIT_PENDING);
	    eventAdd("peerDigestInit", peerDigestInit, p, 0.0, 1);
	}
	return LOOKUP_NONE;
    } else {
	debug(15, 5) ("peerDigestLookup: Whatever!\n");
	return LOOKUP_NONE;
    }
    debug(15, 5) ("peerDigestLookup: OK to lookup peer %s\n", p->host);
    assert(p->digest.cd);
    /* does digest predict a hit? */
    if (!cacheDigestTest(p->digest.cd, key))
	return LOOKUP_MISS;
    debug(15, 5) ("peerDigestLookup: peer %s says HIT!\n", p->host);
    return LOOKUP_HIT;
#endif
    return LOOKUP_NONE;
}

/* select best peer based on cache digests */
peer *
neighborsDigestSelect(request_t * request, StoreEntry * entry)
{
    peer *best_p = NULL;
#if USE_CACHE_DIGESTS
    const cache_key *key;
    int best_rtt = 0;
    int choice_count = 0;
    int ichoice_count = 0;
    peer *p;
    int p_rtt;
    int i;

    key = storeKeyPublic(storeUrl(entry), request->method);
    for (i = 0, p = first_ping; i++ < Config.npeers; p = p->next) {
	lookup_t lookup;
	if (!p)
	    p = Config.peers;
	if (i == 1)
	    first_ping = p;
	lookup = peerDigestLookup(p, request, entry);
	if (lookup == LOOKUP_NONE)
	    continue;
	choice_count++;
	if (lookup == LOOKUP_MISS)
	    continue;
	p_rtt = netdbHostRtt(p->host);
	debug(15, 5) ("neighborsDigestSelect: peer %s rtt: %d\n",
	    p->host, p_rtt);
	/* is this peer better than others in terms of rtt ? */
	if (!best_p || (p_rtt && p_rtt < best_rtt)) {
	    best_p = p;
	    best_rtt = p_rtt;
	    if (p_rtt)		/* informative choice (aka educated guess) */
		ichoice_count++;
	    debug(15, 4) ("neighborsDigestSelect: peer %s leads with rtt %d\n",
		p->host, best_rtt);
	}
    }
    debug(15, 4) ("neighborsDigestSelect: choices: %d (%d)\n",
	choice_count, ichoice_count);
    peerNoteDigestLookup(request, best_p,
	best_p ? LOOKUP_HIT : (choice_count ? LOOKUP_MISS : LOOKUP_NONE));
    request->hier.n_choices = choice_count;
    request->hier.n_ichoices = ichoice_count;
#endif
    return best_p;
}

void
peerNoteDigestLookup(request_t * request, peer * p, lookup_t lookup)
{
#if USE_CACHE_DIGESTS
    if (p)
	strncpy(request->hier.cd_host, p->host, sizeof(request->hier.cd_host));
    else
	*request->hier.cd_host = '\0';
    request->hier.cd_lookup = lookup;
    debug(15, 4) ("peerNoteDigestLookup: peer %s, lookup: %s\n",
	p ? p->host : "<none>", lookup_t_str[lookup]);
#endif
}

static void
neighborAlive(peer * p, const MemObject * mem, const icp_common_t * header)
{
    int rtt;
    int n;
    if (p->stats.logged_state == PEER_DEAD && p->tcp_up) {
	debug(15, 1) ("Detected REVIVED %s: %s/%d/%d\n",
	    neighborTypeStr(p),
	    p->host, p->http_port, p->icp_port);
	p->stats.logged_state = PEER_ALIVE;
    }
    p->stats.last_reply = squid_curtime;
    n = ++p->stats.pings_acked;
    if ((icp_opcode) header->opcode <= ICP_END)
	p->stats.counts[header->opcode]++;
    if (mem) {
	rtt = tvSubMsec(mem->start_ping, current_time);
	p->stats.rtt = intAverage(p->stats.rtt, rtt, n, RTT_AV_FACTOR);
	p->icp_version = (int) header->version;
    }
}

static void
neighborCountIgnored(peer * p, icp_opcode opnotused)
{
    if (p == NULL)
	return;
    p->stats.ignored_replies++;
    NLateReplies++;
}

static peer *non_peers = NULL;

static void
neighborIgnoreNonPeer(const struct sockaddr_in *from, icp_opcode opcode)
{
    peer *np;
    double x;
    for (np = non_peers; np; np = np->next) {
	if (np->in_addr.sin_addr.s_addr != from->sin_addr.s_addr)
	    continue;
	if (np->in_addr.sin_port != from->sin_port)
	    continue;
	break;
    }
    if (np == NULL) {
	np = xcalloc(1, sizeof(peer));
	np->in_addr.sin_addr = from->sin_addr;
	np->in_addr.sin_port = from->sin_port;
	np->icp_port = ntohl(from->sin_port);
	np->type = PEER_NONE;
	np->host = xstrdup(inet_ntoa(from->sin_addr));
	np->next = non_peers;
	non_peers = np;
    }
    np->stats.ignored_replies++;
    np->stats.counts[opcode]++;
    x = log(np->stats.ignored_replies) / log(10.0);
    if (0.0 != x - (double) (int) x)
	return;
    debug(15, 1) ("WARNING: Ignored %d replies from non-peer %s\n",
	np->stats.ignored_replies, np->host);
}

/* ignoreMulticastReply
 * 
 * We want to ignore replies from multicast peers if the
 * cache_host_domain rules would normally prevent the peer
 * from being used
 */
static int
ignoreMulticastReply(peer * p, MemObject * mem)
{
    if (p == NULL)
	return 0;
    if (!EBIT_TEST(p->options, NEIGHBOR_MCAST_RESPONDER))
	return 0;
    if (peerHTTPOkay(p, mem->request))
	return 0;
    return 1;
}

/* I should attach these records to the entry.  We take the first
 * hit we get our wait until everyone misses.  The timeout handler
 * call needs to nip this shopping list or call one of the misses.
 * 
 * If a hit process is already started, then sobeit
 */
void
neighborsUdpAck(const cache_key * key, icp_common_t * header, const struct sockaddr_in *from)
{
    peer *p = NULL;
    StoreEntry *entry;
    MemObject *mem = NULL;
    peer_t ntype = PEER_NONE;
    char *opcode_d;
    icp_opcode opcode = (icp_opcode) header->opcode;

    debug(15, 6) ("neighborsUdpAck: opcode %d '%s'\n",
	(int) opcode, storeKeyText(key));
    if (NULL != (entry = storeGet(key)))
	mem = entry->mem_obj;
    if ((p = whichPeer(from)))
	neighborAlive(p, mem, header);
    if (opcode > ICP_END)
	return;
    opcode_d = icp_opcode_str[opcode];
    /* Does the entry exist? */
    if (NULL == entry) {
	debug(12, 3) ("neighborsUdpAck: Cache key '%s' not found\n",
	    storeKeyText(key));
	neighborCountIgnored(p, opcode);
	return;
    }
    /* check if someone is already fetching it */
    if (EBIT_TEST(entry->flag, ENTRY_DISPATCHED)) {
	debug(15, 3) ("neighborsUdpAck: '%s' already being fetched.\n",
	    storeKeyText(key));
	neighborCountIgnored(p, opcode);
	return;
    }
    if (mem == NULL) {
	debug(15, 2) ("Ignoring %s for missing mem_obj: %s\n",
	    opcode_d, storeKeyText(key));
	neighborCountIgnored(p, opcode);
	return;
    }
    if (entry->ping_status != PING_WAITING) {
	debug(15, 2) ("neighborsUdpAck: Unexpected %s for %s\n",
	    opcode_d, storeKeyText(key));
	neighborCountIgnored(p, opcode);
	return;
    }
    if (entry->lock_count == 0) {
	debug(12, 1) ("neighborsUdpAck: '%s' has no locks\n",
	    storeKeyText(key));
	neighborCountIgnored(p, opcode);
	return;
    }
    debug(15, 3) ("neighborsUdpAck: %s for '%s' from %s \n",
	opcode_d, storeKeyText(key), p ? p->host : "source");
    if (p)
	ntype = neighborType(p, mem->request);
    if (ignoreMulticastReply(p, mem)) {
	neighborCountIgnored(p, opcode);
    } else if (opcode == ICP_SECHO) {
	/* Received source-ping reply */
	if (p) {
	    debug(15, 1) ("Ignoring SECHO from neighbor %s\n", p->host);
	    neighborCountIgnored(p, opcode);
	} else {
	    /* if we reach here, source-ping reply is the first 'parent',
	     * so fetch directly from the source */
	    debug(15, 6) ("Source is the first to respond.\n");
	    mem->icp_reply_callback(NULL, ntype, header, mem->ircb_data);
	}
    } else if (opcode == ICP_MISS) {
	if (p == NULL) {
	    neighborIgnoreNonPeer(from, opcode);
	} else {
	    mem->icp_reply_callback(p, ntype, header, mem->ircb_data);
	}
    } else if (opcode == ICP_HIT) {
	if (p == NULL) {
	    neighborIgnoreNonPeer(from, opcode);
	} else {
	    header->opcode = ICP_HIT;
	    mem->icp_reply_callback(p, ntype, header, mem->ircb_data);
	}
    } else if (opcode == ICP_DECHO) {
	if (p == NULL) {
	    neighborIgnoreNonPeer(from, opcode);
	} else if (ntype == PEER_SIBLING) {
	    debug_trap("neighborsUdpAck: Found non-ICP cache as SIBLING\n");
	    debug_trap("neighborsUdpAck: non-ICP neighbors must be a PARENT\n");
	} else {
	    mem->icp_reply_callback(p, ntype, header, mem->ircb_data);
	}
    } else if (opcode == ICP_SECHO) {
	if (p) {
	    debug(15, 1) ("Ignoring SECHO from neighbor %s\n", p->host);
	    neighborCountIgnored(p, opcode);
#if ALLOW_SOURCE_PING
	} else if (Config.onoff.source_ping) {
	    mem->icp_reply_callback(NULL, ntype, header, mem->ircb_data);
#endif
	} else {
	    debug(15, 1) ("Unsolicited SECHO from %s\n", inet_ntoa(from->sin_addr));
	}
    } else if (opcode == ICP_DENIED) {
	if (p == NULL) {
	    neighborIgnoreNonPeer(from, opcode);
	} else if (p->stats.pings_acked > 100) {
	    if (100 * p->stats.counts[ICP_DENIED] / p->stats.pings_acked > 95) {
		debug(15, 0) ("95%% of replies from '%s' are UDP_DENIED\n", p->host);
		debug(15, 0) ("Disabling '%s', please check your configuration.\n", p->host);
		neighborRemove(p);
		p = NULL;
	    } else {
		neighborCountIgnored(p, opcode);
	    }
	}
    } else if (opcode == ICP_MISS_NOFETCH) {
	mem->icp_reply_callback(p, ntype, header, mem->ircb_data);
    } else {
	debug(15, 0) ("neighborsUdpAck: Unexpected ICP reply: %s\n", opcode_d);
    }
}

peer *
peerFindByName(const char *name)
{
    peer *p = NULL;
    for (p = Config.peers; p; p = p->next) {
	if (!strcasecmp(name, p->host))
	    break;
    }
    return p;
}

int
neighborUp(const peer * p)
{
    if (!p->tcp_up)
	return 0;
    if (squid_curtime - p->stats.last_query > Config.Timeout.deadPeer)
	return 1;
    if (p->stats.last_query - p->stats.last_reply >= Config.Timeout.deadPeer)
	return 0;
    return 1;
}

void
peerDestroy(peer * p)
{
    struct _domain_ping *l = NULL;
    struct _domain_ping *nl = NULL;
    if (p == NULL)
	return;
    for (l = p->pinglist; l; l = nl) {
	nl = l->next;
	safe_free(l->domain);
	safe_free(l);
    }
    safe_free(p->host);
    cbdataFree(p);
}

static void
peerDNSConfigure(const ipcache_addrs * ia, void *data)
{
    peer *p = data;
    struct sockaddr_in *ap;
    int j;
    if (p->n_addresses == 0) {
	debug(15, 1) ("Configuring %s %s/%d/%d\n", neighborTypeStr(p),
	    p->host, p->http_port, p->icp_port);
	if (p->type == PEER_MULTICAST)
	    debug(15, 1) ("    Multicast TTL = %d\n", p->mcast.ttl);
    }
    p->n_addresses = 0;
    if (ia == NULL) {
	debug(0, 0) ("WARNING: DNS lookup for '%s' failed!\n", p->host);
	return;
    }
    if ((int) ia->count < 1) {
	debug(0, 0) ("WARNING: No IP address found for '%s'!\n", p->host);
	return;
    }
    for (j = 0; j < (int) ia->count && j < PEER_MAX_ADDRESSES; j++) {
	p->addresses[j] = ia->in_addrs[j];
	debug(15, 2) ("--> IP address #%d: %s\n", j, inet_ntoa(p->addresses[j]));
	p->n_addresses++;
    }
    ap = &p->in_addr;
    memset(ap, '\0', sizeof(struct sockaddr_in));
    ap->sin_family = AF_INET;
    ap->sin_addr = p->addresses[0];
    ap->sin_port = htons(p->icp_port);
    if (p->type == PEER_MULTICAST)
	peerCountMcastPeersSchedule(p, 10);
    if (p->type != PEER_MULTICAST)
        eventAddIsh("netdbExchangeStart", netdbExchangeStart, p, 30.0, 1);
}

static void
peerRefreshDNS(void *datanotused)
{
    peer *p = NULL;
    if (0 == stat5minClientRequests()) {
	/* no recent client traffic, wait a bit */
        eventAddIsh("peerRefreshDNS", peerRefreshDNS, NULL, 180.0, 1);
	return;
    }
    for (p = Config.peers; p; p = p->next)
	ipcache_nbgethostbyname(p->host, peerDNSConfigure, p);
    /* Reconfigure the peers every hour */
    eventAddIsh("peerRefreshDNS", peerRefreshDNS, NULL, 3600.0, 1);
}

/*
 * peerCheckConnect will NOT be called by eventRun if the peer/data
 * pointer becomes invalid.
 */
static void
peerCheckConnect(void *data)
{
    peer *p = data;
    int fd;
    fd = comm_open(SOCK_STREAM, 0, Config.Addrs.tcp_outgoing,
	0, COMM_NONBLOCKING, p->host);
    if (fd < 0)
	return;
    p->test_fd = fd;
    ipcache_nbgethostbyname(p->host, peerCheckConnect2, p);
}

static void
peerCheckConnect2(const ipcache_addrs * ianotused, void *data)
{
    peer *p = data;
    commConnectStart(p->test_fd,
	p->host,
	p->http_port,
	peerCheckConnectDone,
	p);
}

static void
peerCheckConnectDone(int fd, int status, void *data)
{
    peer *p = data;
    p->tcp_up = status == COMM_OK ? 1 : 0;
    if (p->tcp_up) {
	debug(15, 0) ("TCP connection to %s/%d succeeded\n",
	    p->host, p->http_port);
    } else {
	eventAdd("peerCheckConnect", peerCheckConnect, p, 80.0, 1);
    }
    comm_close(fd);
    return;
}

void
peerCheckConnectStart(peer * p)
{
    if (!p->tcp_up)
	return;
    debug(15, 0) ("TCP connection to %s/%d failed\n", p->host, p->http_port);
    p->tcp_up = 0;
    p->last_fail_time = squid_curtime;
    eventAdd("peerCheckConnect", peerCheckConnect, p, 80.0, 1);
}

static void
peerCountMcastPeersSchedule(peer * p, time_t when)
{
    if (p->mcast.flags & PEER_COUNT_EVENT_PENDING)
	return;
    eventAdd("peerCountMcastPeersStart",
	peerCountMcastPeersStart,
	p,
	(double) when, 1);
    p->mcast.flags |= PEER_COUNT_EVENT_PENDING;
}

static void
peerCountMcastPeersStart(void *data)
{
    peer *p = data;
    ps_state *psstate = xcalloc(1, sizeof(ps_state));
    StoreEntry *fake;
    MemObject *mem;
    icp_common_t *query;
    LOCAL_ARRAY(char, url, MAX_URL);
    assert(p->type == PEER_MULTICAST);
    p->mcast.flags &= ~PEER_COUNT_EVENT_PENDING;
    snprintf(url, MAX_URL, "http://%s/", inet_ntoa(p->in_addr.sin_addr));
    fake = storeCreateEntry(url, url, 0, METHOD_GET);
    psstate->request = requestLink(urlParse(METHOD_GET, url));
    psstate->entry = fake;
    psstate->callback = NULL;
    psstate->fail_callback = NULL;
    psstate->callback_data = p;
    psstate->icp.start = current_time;
    cbdataAdd(psstate, MEM_NONE);
    mem = fake->mem_obj;
    mem->request = requestLink(psstate->request);
    mem->start_ping = current_time;
    mem->icp_reply_callback = peerCountHandleIcpReply;
    mem->ircb_data = psstate;
    mcastSetTtl(theOutIcpConnection, p->mcast.ttl);
    p->mcast.reqnum = mem->reqnum;
    query = icpCreateMessage(ICP_QUERY, 0, url, p->mcast.reqnum, 0);
    icpUdpSend(theOutIcpConnection,
	&p->in_addr,
	query,
	LOG_ICP_QUERY,
	0);
    fake->ping_status = PING_WAITING;
    eventAdd("peerCountMcastPeersDone",
	peerCountMcastPeersDone,
	psstate,
	(double) Config.neighborTimeout, 1);
    p->mcast.flags |= PEER_COUNTING;
    peerCountMcastPeersSchedule(p, MCAST_COUNT_RATE);
}

static void
peerCountMcastPeersDone(void *data)
{
    ps_state *psstate = data;
    peer *p = psstate->callback_data;
    StoreEntry *fake = psstate->entry;
    p->mcast.flags &= ~PEER_COUNTING;
    p->mcast.avg_n_members = doubleAverage(p->mcast.avg_n_members,
	(double) psstate->icp.n_recv,
	++p->mcast.n_times_counted,
	10);
    debug(15, 1) ("Group %s: %d replies, %4.1f average, RTT %d\n",
	p->host,
	psstate->icp.n_recv,
	p->mcast.avg_n_members,
	p->stats.rtt);
    p->mcast.n_replies_expected = (int) p->mcast.avg_n_members;
    fake->store_status = STORE_ABORTED;
    requestUnlink(fake->mem_obj->request);
    fake->mem_obj->request = NULL;
    storeReleaseRequest(fake);
    storeUnlockObject(fake);
    requestUnlink(psstate->request);
    cbdataFree(psstate);
}

static void
peerCountHandleIcpReply(peer * p, peer_t type, icp_common_t * hdrnotused, void *data)
{
    ps_state *psstate = data;
    StoreEntry *fake = psstate->entry;
    MemObject *mem = fake->mem_obj;
    int rtt = tvSubMsec(mem->start_ping, current_time);
    assert(fake);
    assert(mem);
    psstate->icp.n_recv++;
    p->stats.rtt = intAverage(p->stats.rtt, rtt, psstate->icp.n_recv, RTT_AV_FACTOR);
}

static void
neighborDumpPeers(StoreEntry * sentry)
{
    dump_peers(sentry, Config.peers);
}

static void
neighborDumpNonPeers(StoreEntry * sentry)
{
    dump_peers(sentry, non_peers);
}

void
dump_peer_options(StoreEntry * sentry, peer * p)
{
    if (EBIT_TEST(p->options, NEIGHBOR_PROXY_ONLY))
	storeAppendPrintf(sentry, " proxy-only");
    if (EBIT_TEST(p->options, NEIGHBOR_NO_QUERY))
	storeAppendPrintf(sentry, " no-query");
    if (EBIT_TEST(p->options, NEIGHBOR_NO_DIGEST))
	storeAppendPrintf(sentry, " no-digest");
    if (EBIT_TEST(p->options, NEIGHBOR_DEFAULT_PARENT))
	storeAppendPrintf(sentry, " default");
    if (EBIT_TEST(p->options, NEIGHBOR_ROUNDROBIN))
	storeAppendPrintf(sentry, " round-robin");
    if (EBIT_TEST(p->options, NEIGHBOR_MCAST_RESPONDER))
	storeAppendPrintf(sentry, " multicast-responder");
    if (EBIT_TEST(p->options, NEIGHBOR_CLOSEST_ONLY))
	storeAppendPrintf(sentry, " closest-only");
#if USE_HTCP
    if (EBIT_TEST(p->options, NEIGHBOR_HTCP))
	storeAppendPrintf(sentry, " htcp");
#endif
    if (p->mcast.ttl > 0)
	storeAppendPrintf(sentry, " ttl=%d", p->mcast.ttl);
    storeAppendPrintf(sentry, "\n");
}

static void
dump_peers(StoreEntry * sentry, peer * peers)
{
    peer *e = NULL;
    struct _domain_ping *d = NULL;
    icp_opcode op;
    int i;
    if (peers == NULL)
	storeAppendPrintf(sentry, "There are no neighbors installed.\n");
    for (e = peers; e; e = e->next) {
	assert(e->host != NULL);
	storeAppendPrintf(sentry, "\n%-11.11s: %s/%d/%d\n",
	    neighborTypeStr(e),
	    e->host,
	    e->http_port,
	    e->icp_port);
	storeAppendPrintf(sentry, "Flags      :");
	dump_peer_options(sentry, e);
	for (i = 0; i < e->n_addresses; i++) {
	    storeAppendPrintf(sentry, "Address[%d] : %s\n", i,
		inet_ntoa(e->addresses[i]));
	}
	storeAppendPrintf(sentry, "Status     : %s\n",
	    neighborUp(e) ? "Up" : "Down");
	storeAppendPrintf(sentry, "AVG RTT    : %d msec\n", e->stats.rtt);
	storeAppendPrintf(sentry, "LAST QUERY : %8d seconds ago\n",
	    (int) (squid_curtime - e->stats.last_query));
	storeAppendPrintf(sentry, "LAST REPLY : %8d seconds ago\n",
	    (int) (squid_curtime - e->stats.last_reply));
	storeAppendPrintf(sentry, "PINGS SENT : %8d\n", e->stats.pings_sent);
	storeAppendPrintf(sentry, "PINGS ACKED: %8d %3d%%\n",
	    e->stats.pings_acked,
	    percent(e->stats.pings_acked, e->stats.pings_sent));
	storeAppendPrintf(sentry, "FETCHES    : %8d %3d%%\n",
	    e->stats.fetches,
	    percent(e->stats.fetches, e->stats.pings_acked));
	storeAppendPrintf(sentry, "IGNORED    : %8d %3d%%\n",
	    e->stats.ignored_replies,
	    percent(e->stats.ignored_replies, e->stats.pings_acked));
	storeAppendPrintf(sentry, "Histogram of PINGS ACKED:\n");
	for (op = ICP_INVALID; op < ICP_END; op++) {
	    if (e->stats.counts[op] == 0)
		continue;
	    storeAppendPrintf(sentry, "    %12.12s : %8d %3d%%\n",
		icp_opcode_str[op],
		e->stats.counts[op],
		percent(e->stats.counts[op], e->stats.pings_acked));
	}
	if (e->last_fail_time) {
	    storeAppendPrintf(sentry, "Last failed connect() at: %s\n",
		mkhttpdlogtime(&(e->last_fail_time)));
	}
	if (e->pinglist != NULL)
	    storeAppendPrintf(sentry, "DOMAIN LIST: ");
	for (d = e->pinglist; d; d = d->next) {
	    if (d->do_ping)
		storeAppendPrintf(sentry, "%s ", d->domain);
	    else
		storeAppendPrintf(sentry, "!%s ", d->domain);
	}
	storeAppendPrintf(sentry, "\n");
	storeAppendPrintf(sentry, "keep-alive ratio: %d%%\n",
	    percent(e->stats.n_keepalives_recv, e->stats.n_keepalives_sent));
    }
}

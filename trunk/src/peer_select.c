
/*
 * $Id$
 *
 * DEBUG: section 44    Peer Selection Algorithm
 * AUTHOR: Duane Wessels
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

const char *hier_strings[] =
{
    "NONE",
    "DIRECT",
    "SIBLING_HIT",
    "PARENT_HIT",
    "DEFAULT_PARENT",
    "SINGLE_PARENT",
    "FIRST_UP_PARENT",
    "NO_PARENT_DIRECT",
    "FIRST_PARENT_MISS",
    "CLOSEST_PARENT_MISS",
    "CLOSEST_PARENT",
    "CLOSEST_DIRECT",
    "NO_DIRECT_FAIL",
    "SOURCE_FASTEST",
    "ROUNDROBIN_PARENT",
#if USE_CACHE_DIGESTS
    "CACHE_DIGEST_HIT",
    "NO_CACHE_DIGEST_DIRECT",
#endif
#if USE_CARP
    "CARP",
#endif
    "INVALID CODE"
};

static struct {
    int timeouts;
} PeerStats;

static char *DirectStr[] =
{
    "DIRECT_NO",
    "DIRECT_MAYBE",
    "DIRECT_YES"
};

static void peerSelectFoo(ps_state *);
static void peerPingTimeout(void *data);
static void peerSelectCallbackFail(ps_state * psstate);
static IRCB peerHandleIcpReply;
static void peerSelectStateFree(ps_state * psstate);
static void peerIcpParentMiss(peer *, icp_common_t *, ps_state *);
static int peerCheckNetdbDirect(ps_state * psstate);

static void
peerSelectStateFree(ps_state * psstate)
{
    if (psstate->acl_checklist) {
	debug(44, 1) ("calling aclChecklistFree() from peerSelectStateFree\n");
	aclChecklistFree(psstate->acl_checklist);
    }
    requestUnlink(psstate->request);
    psstate->request = NULL;
    if (psstate->entry) {
	storeUnlockObject(psstate->entry);
	psstate->entry = NULL;
    }
    cbdataFree(psstate);
}

int
peerSelectIcpPing(request_t * request, int direct, StoreEntry * entry)
{
    int n;
    if (entry == NULL)
	return 0;
    debug(44, 3) ("peerSelectIcpPing: %s\n", storeUrl(entry));
    if (entry->ping_status != PING_NONE)
	return 0;
    assert(direct != DIRECT_YES);
    if (!request->flags.hierarchical && direct != DIRECT_NO)
	return 0;
    if (EBIT_TEST(entry->flag, KEY_PRIVATE) && !neighbors_do_private_keys)
	if (direct != DIRECT_NO)
	    return 0;
    n = neighborsCount(request);
    debug(44, 3) ("peerSelectIcpPing: counted %d neighbors\n", n);
    return n;
}


peer *
peerGetSomeParent(request_t * request, hier_code * code)
{
    peer *p;
    debug(44, 3) ("peerGetSomeParent: %s %s\n",
	RequestMethodStr[request->method],
	request->host);
    if ((p = getDefaultParent(request))) {
	*code = DEFAULT_PARENT;
	return p;
    }
    if ((p = getRoundRobinParent(request))) {
	*code = ROUNDROBIN_PARENT;
	return p;
    }
    if ((p = getFirstUpParent(request))) {
	*code = FIRSTUP_PARENT;
	return p;
    }
    return NULL;
}

void
peerSelect(request_t * request,
    StoreEntry * entry,
    PSC * callback,
    PSC * fail_callback,
    void *callback_data)
{
    ps_state *psstate = xcalloc(1, sizeof(ps_state));
    if (entry)
	debug(44, 3) ("peerSelect: %s\n", storeUrl(entry));
    else
	debug(44, 3) ("peerSelect: %s\n", RequestMethodStr[request->method]);
    cbdataAdd(psstate, MEM_NONE);
    psstate->request = requestLink(request);
    psstate->entry = entry;
    psstate->callback = callback;
    psstate->fail_callback = fail_callback;
    psstate->callback_data = callback_data;
#if USE_CACHE_DIGESTS
    request->hier.peer_select_start = current_time;
#endif
    if (psstate->entry)
	storeLockObject(psstate->entry);
    cbdataLock(callback_data);
    peerSelectFoo(psstate);
}

static void
peerCheckNeverDirectDone(int answer, void *data)
{
    ps_state *psstate = data;
    psstate->acl_checklist = NULL;
    debug(44, 3) ("peerCheckNeverDirectDone: %d\n", answer);
    psstate->never_direct = answer ? 1 : -1;
    peerSelectFoo(psstate);
}

static void
peerCheckAlwaysDirectDone(int answer, void *data)
{
    ps_state *psstate = data;
    psstate->acl_checklist = NULL;
    debug(44, 3) ("peerCheckAlwaysDirectDone: %d\n", answer);
    psstate->always_direct = answer ? 1 : -1;
    peerSelectFoo(psstate);
}

static void
peerSelectCallback(ps_state * psstate, peer * p)
{
    StoreEntry *entry = psstate->entry;
    void *data = psstate->callback_data;
    if (entry) {
	debug(44, 3) ("peerSelectCallback: %s\n", storeUrl(entry));
	if (entry->ping_status == PING_WAITING)
	    eventDelete(peerPingTimeout, psstate);
	entry->ping_status = PING_DONE;
    }
    psstate->icp.stop = current_time;
    if (cbdataValid(data))
	psstate->callback(p, data);
    cbdataUnlock(data);
    peerSelectStateFree(psstate);
}

static void
peerSelectCallbackFail(ps_state * psstate)
{
    request_t *request = psstate->request;
    void *data = psstate->callback_data;
    const char *url = psstate->entry ? storeUrl(psstate->entry) : urlCanonical(request);
    debug(44, 1) ("Failed to select source for '%s'\n", url);
    debug(44, 1) ("  always_direct = %d\n", psstate->always_direct);
    debug(44, 1) ("   never_direct = %d\n", psstate->never_direct);
    debug(44, 1) ("       timedout = %d\n", psstate->icp.timedout);
    if (cbdataValid(data))
	psstate->fail_callback(NULL, data);
    cbdataUnlock(data);
    peerSelectStateFree(psstate);
    /* XXX When this happens, the client request just hangs */
}

static int
peerCheckNetdbDirect(ps_state * psstate)
{
    peer *p = whichPeer(&psstate->closest_parent_miss);
    int myrtt;
    int myhops;
    if (p == NULL)
	return 0;
    myrtt = netdbHostRtt(psstate->request->host);
    debug(44, 3) ("peerCheckNetdbDirect: MY RTT = %d msec\n", myrtt);
    debug(44, 3) ("peerCheckNetdbDirect: closest_parent_miss RTT = %d msec\n",
	psstate->icp.p_rtt);
    if (myrtt && myrtt < psstate->icp.p_rtt)
	return 1;
    myhops = netdbHostHops(psstate->request->host);
    debug(44, 3) ("peerCheckNetdbDirect: MY hops = %d\n", myhops);
    debug(44, 3) ("peerCheckNetdbDirect: minimum_direct_hops = %d\n",
	Config.minDirectHops);
    if (myhops && myhops <= Config.minDirectHops)
	return 1;
    return 0;
}

static void
peerSelectFoo(ps_state * psstate)
{
    peer *p;
    hier_code code;
    StoreEntry *entry = psstate->entry;
    request_t *request = psstate->request;
    int direct;
    debug(44, 3) ("peerSelectFoo: '%s %s'\n",
	RequestMethodStr[request->method],
	request->host);
    if (psstate->never_direct == 0 && Config.accessList.NeverDirect) {
	psstate->acl_checklist = aclChecklistCreate(
	    Config.accessList.NeverDirect,
	    request,
	    request->client_addr,
	    NULL,		/* user agent */
	    NULL);		/* ident */
	aclNBCheck(psstate->acl_checklist,
	    peerCheckNeverDirectDone,
	    psstate);
	return;
    } else if (psstate->never_direct > 0) {
	direct = DIRECT_NO;
    } else if (psstate->always_direct == 0 && Config.accessList.AlwaysDirect) {
	psstate->acl_checklist = aclChecklistCreate(
	    Config.accessList.AlwaysDirect,
	    request,
	    request->client_addr,
	    NULL,		/* user agent */
	    NULL);		/* ident */
	aclNBCheck(psstate->acl_checklist,
	    peerCheckAlwaysDirectDone,
	    psstate);
	return;
    } else if (psstate->always_direct > 0) {
	direct = DIRECT_YES;
    } else if (request->flags.loopdetect) {
	direct = DIRECT_YES;
    } else {
	direct = DIRECT_MAYBE;
    }
    debug(44, 3) ("peerSelectFoo: direct = %s\n", DirectStr[direct]);
    if (direct == DIRECT_YES) {
	debug(44, 3) ("peerSelectFoo: DIRECT\n");
	hierarchyNote(&request->hier, DIRECT, &psstate->icp, request->host);
	peerSelectCallback(psstate, NULL);
	return;
    }
    if ((p = getSingleParent(request))) {
	psstate->single_parent = p->in_addr;
	debug(44, 3) ("peerSelect: found single parent, skipping ICP query\n");
    }
#if USE_CACHE_DIGESTS
    else if ((p = neighborsDigestSelect(request, entry))) {
	debug(44, 2) ("peerSelect: Using Cache Digest\n");
	request->hier.alg = PEER_SA_DIGEST;
	code = CACHE_DIGEST_HIT;
	debug(44, 2) ("peerSelect: %s/%s\n", hier_strings[code], p->host);
	hierarchyNote(&request->hier, code, &psstate->icp, p->host);
	peerSelectCallback(psstate, p);
	return;
    }
#endif
#if USE_CARP
    else if ((p = carpSelectParent(request))) {
	hierarchyNote(&request->hier, CARP, &psstate->icp, p->host);
	peerSelectCallback(psstate, p);
	return;
    }
#endif
    else if ((p = netdbClosestParent(request))) {
	request->hier.alg = PEER_SA_NETDB;
	code = CLOSEST_PARENT;
	debug(44, 2) ("peerSelect: %s/%s\n", hier_strings[code], p->host);
	hierarchyNote(&request->hier, code, &psstate->icp, p->host);
	peerSelectCallback(psstate, p);
	return;
    } else if (peerSelectIcpPing(request, direct, entry)) {
	assert(entry->ping_status == PING_NONE);
	request->hier.alg = PEER_SA_ICP;
	debug(44, 3) ("peerSelect: Doing ICP pings\n");
	psstate->icp.start = current_time;
	psstate->icp.n_sent = neighborsUdpPing(request,
	    entry,
	    peerHandleIcpReply,
	    psstate,
	    &psstate->icp.n_replies_expected,
	    &psstate->icp.timeout);
	if (psstate->icp.n_sent == 0)
	    debug(44, 0) ("WARNING: neighborsUdpPing returned 0\n");
	debug(44, 3) ("peerSelectFoo: %d ICP replies expected, RTT %d msec\n",
	    psstate->icp.n_replies_expected, psstate->icp.timeout);
	if (psstate->icp.n_replies_expected > 0) {
	    entry->ping_status = PING_WAITING;
	    eventAdd("peerPingTimeout",
		peerPingTimeout,
		psstate,
		0.001 * psstate->icp.timeout,
		0);
	    return;
	}
    }
    debug(44, 3) ("peerSelectFoo: After peerSelectIcpPing.\n");
    if (peerCheckNetdbDirect(psstate)) {
	code = CLOSEST_DIRECT;
	debug(44, 3) ("peerSelect: %s/%s\n", hier_strings[code], request->host);
	hierarchyNote(&request->hier, code, &psstate->icp, request->host);
	peerSelectCallback(psstate, NULL);
    } else if ((p = whichPeer(&psstate->closest_parent_miss))) {
	code = CLOSEST_PARENT_MISS;
	debug(44, 3) ("peerSelect: %s/%s\n", hier_strings[code], p->host);
	hierarchyNote(&request->hier, code, &psstate->icp, p->host);
	peerSelectCallback(psstate, p);
    } else if ((p = whichPeer(&psstate->first_parent_miss))) {
	code = FIRST_PARENT_MISS;
	debug(44, 3) ("peerSelect: %s/%s\n", hier_strings[code], p->host);
	hierarchyNote(&request->hier, code, &psstate->icp, p->host);
	peerSelectCallback(psstate, p);
    } else if ((p = whichPeer(&psstate->single_parent))) {
	code = SINGLE_PARENT;
	debug(44, 3) ("peerSelect: %s/%s\n", hier_strings[code], p->host);
	hierarchyNote(&request->hier, code, &psstate->icp, p->host);
	peerSelectCallback(psstate, p);
    } else if (direct != DIRECT_NO) {
	code = DIRECT;
	debug(44, 3) ("peerSelect: %s/%s\n", hier_strings[code], request->host);
	hierarchyNote(&request->hier, code, &psstate->icp, request->host);
	peerSelectCallback(psstate, NULL);
    } else if ((p = peerGetSomeParent(request, &code))) {
	debug(44, 3) ("peerSelect: %s/%s\n", hier_strings[code], p->host);
	hierarchyNote(&request->hier, code, &psstate->icp, p->host);
	peerSelectCallback(psstate, p);
    } else {
	code = NO_DIRECT_FAIL;
	hierarchyNote(&request->hier, code, &psstate->icp, NULL);
	peerSelectCallbackFail(psstate);
    }
}

static void
peerPingTimeout(void *data)
{
    ps_state *psstate = data;
    StoreEntry *entry = psstate->entry;
    if (!cbdataValid(psstate->callback_data)) {
	/* request aborted */
	cbdataUnlock(psstate->callback_data);
	peerSelectStateFree(psstate);
	return;
    }
    if (entry)
	debug(44, 3) ("peerPingTimeout: '%s'\n", storeUrl(entry));
    entry->ping_status = PING_TIMEOUT;
    PeerStats.timeouts++;
    psstate->icp.timedout = 1;
    peerSelectFoo(psstate);
}

void
peerSelectInit(void)
{
    memset(&PeerStats, '\0', sizeof(PeerStats));
    assert(sizeof(hier_strings) == (HIER_MAX + 1) * sizeof(char *));
}

static void
peerIcpParentMiss(peer * p, icp_common_t * header, ps_state * ps)
{
    int rtt;
    int hops;
    if (Config.onoff.query_icmp) {
	if (header->flags & ICP_FLAG_SRC_RTT) {
	    rtt = header->pad & 0xFFFF;
	    hops = (header->pad >> 16) & 0xFFFF;
	    if (rtt > 0 && rtt < 0xFFFF)
		netdbUpdatePeer(ps->request, p, rtt, hops);
	    if (rtt && (ps->icp.p_rtt == 0 || rtt < ps->icp.p_rtt)) {
		ps->closest_parent_miss = p->in_addr;
		ps->icp.p_rtt = rtt;
	    }
	}
    }
    /* if closest-only is set, then don't allow FIRST_PARENT_MISS */
    if (EBIT_TEST(p->options, NEIGHBOR_CLOSEST_ONLY))
	return;
    /* set FIRST_MISS if there is no CLOSEST parent */
    if (ps->closest_parent_miss.sin_addr.s_addr != any_addr.s_addr)
	return;
    rtt = tvSubMsec(ps->icp.start, current_time) / p->weight;
    if (ps->icp.w_rtt == 0 || rtt < ps->icp.w_rtt) {
	ps->first_parent_miss = p->in_addr;
	ps->icp.w_rtt = rtt;
    }
}

static void
peerHandleIcpReply(peer * p, peer_t type, icp_common_t * header, void *data)
{
    ps_state *psstate = data;
    icp_opcode op = header->opcode;
    request_t *request = psstate->request;
    debug(44, 3) ("peerHandleIcpReply: %s %s\n",
	icp_opcode_str[op],
	storeUrl(psstate->entry));
#if USE_CACHE_DIGESTS && 0
    /* do cd lookup to count false misses */
    if (p && request)
	peerNoteDigestLookup(request, p,
	    peerDigestLookup(p, request, psstate->entry));
#endif
    psstate->icp.n_recv++;
    if (op == ICP_MISS || op == ICP_DECHO) {
	if (type == PEER_PARENT)
	    peerIcpParentMiss(p, header, psstate);
    } else if (op == ICP_HIT) {
	hierarchyNote(&request->hier,
	    type == PEER_PARENT ? PARENT_HIT : SIBLING_HIT,
	    &psstate->icp,
	    p->host);
	peerSelectCallback(psstate, p);
	return;
    } else if (op == ICP_SECHO) {
	hierarchyNote(&request->hier,
	    SOURCE_FASTEST,
	    &psstate->icp,
	    request->host);
	peerSelectCallback(psstate, NULL);
	return;
    }
    if (psstate->icp.n_recv < psstate->icp.n_replies_expected)
	return;
    peerSelectFoo(psstate);
}

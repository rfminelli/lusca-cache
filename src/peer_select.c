
/*
 * $Id$
 *
 * DEBUG: section 44    Peer Selection Algorithm
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
    "CLOSEST_DIRECT",
    "NO_DIRECT_FAIL",
    "SOURCE_FASTEST",
    "SIBLING_UDP_HIT_OBJ",
    "PARENT_UDP_HIT_OBJ",
    "PASSTHROUGH_PARENT",
    "SSL_PARENT_MISS",
    "ROUNDROBIN_PARENT",
    "INVALID CODE"
};

static struct {
    int timeouts;
} PeerStats;


static void peerSelectFoo _PARAMS((ps_state *));
static void peerPingTimeout _PARAMS((void *data));
static void peerSelectCallbackFail _PARAMS((ps_state * psstate));
static void peerHandleIcpReply _PARAMS((peer * p, peer_t type, icp_opcode op, void *data));
static void peerSelectStateFree _PARAMS((ps_state * psstate));

static void
peerSelectStateFree(ps_state * psstate)
{
    if (psstate->acl_checklist) {
	debug(0, 0) ("calling aclChecklistFree() from peerSelectStateFree\n");
	aclChecklistFree(psstate->acl_checklist);
    }
    requestUnlink(psstate->request);
    psstate->request = NULL;
    cbdataFree(psstate);
}

int
peerSelectIcpPing(request_t * request, int direct, StoreEntry * entry)
{
    if (entry == NULL)
	return 0;
    if (entry->ping_status != PING_NONE)
	return 0;
    if (direct == DIRECT_YES)
	fatal_dump("direct == DIRECT_YES");
    if (!BIT_TEST(entry->flag, HIERARCHICAL) && direct != DIRECT_NO)
	return 0;
    if (Config.singleParentBypass && !Config.sourcePing)
	if (getSingleParent(request))
	    return 0;
    if (BIT_TEST(entry->flag, KEY_PRIVATE) && !neighbors_do_private_keys)
	if (direct != DIRECT_NO)
	    return 0;
    return neighborsCount(request);
}


peer *
peerGetSomeParent(request_t * request, hier_code * code)
{
    peer *p;
    if (request->method == METHOD_CONNECT)
	if ((p = getSslParent())) {
	    *code = SSL_PARENT;
	    return p;
	}
    if (request->method != METHOD_GET)
	if ((p = getPassParent())) {
	    *code = PASS_PARENT;
	    return p;
	}
    if ((p = getDefaultParent(request))) {
	*code = DEFAULT_PARENT;
	return p;
    }
    if ((p = getSingleParent(request))) {
	*code = SINGLE_PARENT;
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
    cbdataAdd(psstate);
    psstate->request = requestLink(request);
    psstate->entry = entry;
    psstate->callback = callback;
    psstate->fail_callback = fail_callback;
    psstate->callback_data = callback_data;
    cbdataLock(callback_data);
    psstate->icp.start = current_time;
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
	if (entry->ping_status == PING_WAITING)
	    eventDelete(peerPingTimeout, psstate);
	entry->ping_status = PING_DONE;
    }
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
    char *url = psstate->entry ? psstate->entry->url : urlCanonical(request, NULL);
    debug(44, 1) ("Failed to select source for '%s'\n", url);
    debug(44, 1) ("  always_direct = %d\n", psstate->always_direct);
    debug(44, 1) ("   never_direct = %d\n", psstate->never_direct);
    debug(44, 1) ("        timeout = %d\n", psstate->icp.timeout);
    if (cbdataValid(data))
	psstate->fail_callback(NULL, data);
    cbdataUnlock(data);
    peerSelectStateFree(psstate);
}

static void
peerSelectFoo(ps_state * psstate)
{
    peer *p;
    hier_code code;
    StoreEntry *entry = psstate->entry;
    request_t *request = psstate->request;
    int direct;
    debug(44, 3) ("peerSelect: '%s %s'\n",
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
    } else {
	direct = DIRECT_MAYBE;
    }
    debug(44, 3) ("peerSelect: direct = %d\n", direct);
    if (direct == DIRECT_YES) {
	debug(44, 3) ("peerSelect: DIRECT\n");
	hierarchyNote(&request->hier, DIRECT, &psstate->icp, request->host);
	peerSelectCallback(psstate, NULL);
	return;
    }
    if (peerSelectIcpPing(request, direct, entry)) {
	if (entry->ping_status != PING_NONE)
	    fatal_dump("peerSelectFoo: bad ping_status");
	debug(44, 3) ("peerSelect: Doing ICP pings\n");
	psstate->icp.n_sent = neighborsUdpPing(request,
	    entry,
	    peerHandleIcpReply,
	    psstate,
	    &psstate->icp.n_replies_expected);
	if (psstate->icp.n_sent > 0) {
	    entry->ping_status = PING_WAITING;
	    eventAdd("peerPingTimeout",
		peerPingTimeout,
		psstate,
		Config.neighborTimeout);
	    return;
	}
	debug_trap("peerSelect: neighborsUdpPing returned 0");
    }
    if ((p = psstate->first_parent_miss)) {
	code = FIRST_PARENT_MISS;
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

void
peerPingTimeout(void *data)
{
    ps_state *psstate = data;
    StoreEntry *entry = psstate->entry;
    debug(44, 3) ("peerPingTimeout: '%s'\n", entry->url);
    entry->ping_status = PING_TIMEOUT;
    PeerStats.timeouts++;
    psstate->icp.timeout = 1;
    peerSelectFoo(psstate);
}

void
peerSelectInit(void)
{
    memset(&PeerStats, '\0', sizeof(PeerStats));
}


static void
peerHandleIcpReply(peer * p, peer_t type, icp_opcode op, void *data)
{
    ps_state *psstate = data;
    int w_rtt;
    request_t *request = psstate->request;
    psstate->icp.n_recv++;
    if (op == ICP_OP_MISS || op == ICP_OP_DECHO) {
	if (type == PEER_PARENT) {
	    w_rtt = tvSubMsec(psstate->icp.start, current_time) / p->weight;
	    if (psstate->icp.w_rtt == 0 || w_rtt < psstate->icp.w_rtt) {
		psstate->first_parent_miss = p;
		psstate->icp.w_rtt = w_rtt;
	    }
	}
    } else if (op == ICP_OP_HIT || op == ICP_OP_HIT_OBJ) {
	hierarchyNote(&request->hier,
	    type == PEER_PARENT ? PARENT_HIT : SIBLING_HIT,
	    &psstate->icp,
	    p->host);
	peerSelectCallback(psstate, p);
	return;
    } else if (op == ICP_OP_SECHO) {
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

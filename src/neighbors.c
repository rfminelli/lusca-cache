/*
 * $Id$
 *
 * DEBUG: section 15    Neighbor Routines
 * AUTHOR: Harvest Derived
 *
 * SQUID Internet Object Cache  http://www.nlanr.net/Squid/
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

static int edgeWouldBePinged _PARAMS((edge *, request_t *));
static void neighborRemove _PARAMS((edge *));

static neighbors *friends = NULL;
static struct neighbor_cf *Neighbor_cf = NULL;
static icp_common_t echo_hdr;
static u_short echo_port;
static struct in_addr any_addr;

FILE *cache_hierarchy_log = NULL;

char *hier_strings[] =
{
    "NONE",
    "DIRECT",
    "NEIGHBOR_HIT",
    "PARENT_HIT",
    "SINGLE_PARENT",
    "FIRST_UP_PARENT",
    "NO_PARENT_DIRECT",
    "FIRST_PARENT_MISS",
    "LOCAL_IP_DIRECT",
    "FIREWALL_IP_DIRECT",
    "DEAD_PARENT",
    "DEAD_NEIGHBOR",
    "REVIVE_PARENT",
    "REVIVE_NEIGHBOR",
    "NO_DIRECT_FAIL",
    "SOURCE_FASTEST",
    "UDP_HIT_OBJ",
    "INVALID CODE"
};


edge *whichEdge(header, from)
     icp_common_t *header;
     struct sockaddr_in *from;
{
    int j;
    u_short port;
    struct in_addr ip;
    edge *e = NULL;

    port = ntohs(from->sin_port);
    ip = from->sin_addr;

    debug(15, 3, "whichEdge: from %s port %d\n", inet_ntoa(ip), port);

    for (e = friends->edges_head; e; e = e->next) {
	for (j = 0; j < e->n_addresses; j++) {
	    if (ip.s_addr == e->addresses[j].s_addr && port == e->icp_port) {
		return e;
	    }
	}
    }
    return (NULL);
}


void hierarchy_log_append(entry, code, timeout, cache_host)
     StoreEntry *entry;
     hier_code code;
     int timeout;
     char *cache_host;
{
    char *url = entry->url;
    MemObject *mem = entry->mem_obj;
    static time_t last_time = 0;
    static char time_str[128];
    char *s = NULL;

    if (code > HIER_MAX)
	code = HIER_MAX;
    if (mem)
	mem->request->hierarchy_code = code;

    if (!cache_hierarchy_log)
	return;

    if (emulate_httpd_log) {
	if (squid_curtime != last_time) {
	    s = mkhttpdlogtime(&squid_curtime);
	    strcpy(time_str, s);
	    last_time = squid_curtime;
	}
	if (cache_host) {
	    fprintf(cache_hierarchy_log, "[%s] %s %s%s %s\n",
		time_str,
		url,
		timeout ? "TIMEOUT_" : "",
		hier_strings[code],
		cache_host);
	} else {
	    fprintf(cache_hierarchy_log, "[%s] %s %s%s\n",
		time_str,
		url,
		timeout ? "TIMEOUT_" : "",
		hier_strings[code]);
	}
    } else {
	if (cache_host) {
	    fprintf(cache_hierarchy_log, "%d.%03d %s %s%s %s\n",
		(int) current_time.tv_sec,
		(int) current_time.tv_usec / 1000,
		url,
		timeout ? "TIMEOUT_" : "",
		hier_strings[code],
		cache_host);
	} else {
	    fprintf(cache_hierarchy_log, "%d.%03d %s %s%s\n",
		(int) current_time.tv_sec,
		(int) current_time.tv_usec / 1000,
		url,
		timeout ? "TIMEOUT_" : "",
		hier_strings[code]);
	}
    }
    if (unbuffered_logs)
	fflush(cache_hierarchy_log);
}

static int edgeWouldBePinged(e, request)
     edge *e;
     request_t *request;
{
    dom_list *d = NULL;
    int do_ping = 1;
    struct _acl_list *a = NULL;

    if (e->type == EDGE_SIBLING && BIT_TEST(request->flags, REQ_NOCACHE))
	return 0;
    if (e->domains == NULL && e->acls == NULL)
	return do_ping;
    do_ping = 0;
    for (d = e->domains; d; d = d->next) {
	if (matchDomainName(d->domain, request->host))
	    return d->do_ping;
	do_ping = !d->do_ping;
    }
    for (a = e->acls; a; a = a->next) {
	if (aclMatchAcl(a->acl, any_addr, request))
	    return a->op;
	do_ping = !a->op;
    }
    return do_ping;
}

edge *getSingleParent(request, n)
     request_t *request;
     int *n;
{
    edge *p = NULL;
    edge *e = NULL;
    int count = 0;

    if (n == NULL && friends->n_parent < 1)
	return NULL;
    for (e = friends->edges_head; e; e = e->next) {
	if (!edgeWouldBePinged(e, request))
	    continue;
	count++;
	if (e->type != EDGE_PARENT) {
	    /* we matched a neighbor, not a parent.  There
	     * can be no single parent */
	    if (n == NULL)
		return NULL;
	    continue;
	}
	if (p) {
	    /* already have a parent, this makes the second,
	     * so there can be no single parent */
	    if (n == NULL)
		return NULL;
	    continue;
	}
	p = e;
    }
    /* Ok, all done checking the edges.  If only one parent matched, then
     * p will already point to it */
    if (n)
	*n = count;
    if (count == 1)
	return p;
    return NULL;
}

edge *getFirstUpParent(request)
     request_t *request;
{
    edge *e = NULL;
    if (friends->n_parent < 1)
	return NULL;
    for (e = friends->edges_head; e; e = e->next) {
	if (!e->neighbor_up)
	    continue;
	if (e->type != EDGE_PARENT)
	    continue;
	if (edgeWouldBePinged(e, request))
	    return e;
    }
    return NULL;
}

edge *getNextEdge(edge * e)
{
    return e->next;
}

edge *getFirstEdge()
{
    return friends->edges_head;
}

void neighborRemove(target)
     edge *target;
{
    edge *e = NULL;
    edge **E = NULL;
    e = friends->edges_head;
    E = &friends->edges_head;
    while (e) {
	if (target == e)
	    break;
	E = &e->next;
	e = e->next;
    }
    if (e) {
	*E = e->next;
	safe_free(e->host);
	safe_free(e);
	friends->n--;
    }
}

void neighborsDestroy()
{
    edge *e = NULL;
    edge *next = NULL;

    debug(15, 3, "neighborsDestroy: called\n");

    for (e = friends->edges_head; e; e = next) {
	next = e->next;
	safe_free(e->host);
	/* XXX I think we need to free e->domains too -DW */
	safe_free(e);
    }
    safe_free(friends);
    friends = NULL;
}

static void neighborsOpenLog(fname)
     char *fname;
{
    int log_fd = -1;
    /* Close and reopen the log.  It may have been renamed "manually"
     * before HUP'ing us. */
    if (cache_hierarchy_log) {
	file_close(fileno(cache_hierarchy_log));
	fclose(cache_hierarchy_log);
	cache_hierarchy_log = NULL;
    }
    if (strcmp(fname, "none") != 0) {
	log_fd = file_open(fname, NULL, O_WRONLY | O_CREAT | O_APPEND);
	if (log_fd < 0) {
	    debug(15, 0, "neighborsOpenLog: %s: %s\n", fname, xstrerror());
	} else if ((cache_hierarchy_log = fdopen(log_fd, "a")) == NULL) {
	    file_close(log_fd);
	    debug(15, 0, "neighborsOpenLog: %s: %s\n", fname, xstrerror());
	}
    }
    if (log_fd < 0 || cache_hierarchy_log == NULL)
	debug(15, 1, "Hierachical logging is disabled.\n");
}

void neighbors_open(fd)
     int fd;
{
    int j;
    struct sockaddr_in name;
    struct sockaddr_in *ap;
    int len = sizeof(struct sockaddr_in);
    char **list = NULL;
    edge *e = NULL;
    edge *next = NULL;
    edge **E = NULL;
    struct in_addr *ina = NULL;
    struct servent *sep = NULL;

    memset(&name, '\0', sizeof(struct sockaddr_in));
    if (getsockname(fd, (struct sockaddr *) &name, &len) < 0)
	debug(15, 1, "getsockname(%d,%p,%p) failed.\n", fd, &name, &len);

    /* Prepare neighbor connections, one at a time */
    E = &friends->edges_head;
    next = friends->edges_head;
    while ((e = next)) {
	next = e->next;
	debug(15, 2, "Finding IP addresses for '%s'\n", e->host);
	if ((list = getAddressList(e->host)) == NULL) {
	    debug(0, 0, "WARNING!!: DNS lookup for '%s' failed!\n", e->host);
	    debug(0, 0, "THIS NEIGHBOR WILL BE IGNORED.\n");
	    *E = next;		/* skip */
	    safe_free(e);
	    continue;
	}
	e->n_addresses = 0;
	for (j = 0; *list && j < EDGE_MAX_ADDRESSES; j++) {
	    ina = &e->addresses[j];
	    xmemcpy(&(ina->s_addr), *list, 4);
	    list++;
	    e->n_addresses++;
	}
	if (e->n_addresses < 1) {
	    debug(0, 0, "WARNING!!: No IP address found for '%s'!\n", e->host);
	    debug(0, 0, "THIS NEIGHBOR WILL BE IGNORED.\n");
	    *E = next;		/* skip */
	    safe_free(e);
	    continue;
	}
	for (j = 0; j < e->n_addresses; j++) {
	    debug(15, 2, "--> IP address #%d: %s\n",
		j, inet_ntoa(e->addresses[j]));
	}
	e->stats.rtt = 0;

	ap = &e->in_addr;
	memset(ap, '\0', sizeof(struct sockaddr_in));
	ap->sin_family = AF_INET;
	ap->sin_addr = e->addresses[0];
	ap->sin_port = htons(e->icp_port);

	if (e->type == EDGE_PARENT) {
	    debug(15, 3, "parent_install: host %s addr %s port %d\n",
		e->host, inet_ntoa(ap->sin_addr),
		e->icp_port);
	    e->neighbor_up = 1;
	} else {
	    debug(15, 3, "neighbor_install: host %s addr %s port %d\n",
		e->host, inet_ntoa(ap->sin_addr),
		e->icp_port);
	    e->neighbor_up = 1;
	}
	E = &e->next;
    }

    if (0 == echo_hdr.opcode) {
	echo_hdr.opcode = ICP_OP_SECHO;
	echo_hdr.version = ICP_VERSION_CURRENT;
	echo_hdr.length = 0;
	echo_hdr.reqnum = 0;
	echo_hdr.flags = 0;
	echo_hdr.pad = 0;
	/* memset(echo_hdr.auth, '\0', sizeof(u_num32) * ICP_AUTH_SIZE); */
	echo_hdr.shostid = name.sin_addr.s_addr;
	sep = getservbyname("echo", "udp");
	echo_port = sep ? ntohs((u_short) sep->s_port) : 7;
    }
}


int neighborsUdpPing(proto)
     protodispatch_data *proto;
{
    char *host = proto->request->host;
    char *url = proto->url;
    StoreEntry *entry = proto->entry;
    struct hostent *hep = NULL;
    struct sockaddr_in to_addr;
    edge *e = NULL;
    int i;
    MemObject *mem = entry->mem_obj;
    int reqnum = 0;
    int flags;

    mem->e_pings_n_pings = 0;
    mem->e_pings_n_acks = 0;
    mem->e_pings_first_miss = NULL;
    mem->w_rtt = 0;
    mem->start_ping = current_time;

    if (friends->edges_head == NULL)
	return 0;
    if (theOutIcpConnection < 0) {
	debug(15, 0, "neighborsUdpPing: There is no ICP socket!\n");
	debug(15, 0, "Cannot query neighbors for '%s'.\n", url);
	debug(15, 0, "Check 'icp_port' in your config file\n");
	fatal_dump(NULL);
    }
    for (i = 0, e = friends->first_ping; i++ < friends->n; e = e->next) {
	if (e == (edge *) NULL)
	    e = friends->edges_head;
	debug(15, 5, "neighborsUdpPing: %s %s\n",
	    e->type == EDGE_PARENT ? "PARENT" : "SIBLING",
	    e->host);

	/* skip any cache where we failed to connect() w/in the last 60s */
	if (squid_curtime - e->last_fail_time < 60)
	    continue;

	if (!edgeWouldBePinged(e, proto->request))
	    continue;		/* next edge */
	if (e->options & NEIGHBOR_NO_QUERY)
	    continue;

	debug(15, 4, "neighborsUdpPing: pinging cache %s for <URL:%s>\n",
	    e->host, url);

	if (BIT_TEST(entry->flag, KEY_PRIVATE))
	    reqnum = atoi(entry->key);
	else
	    reqnum = getKeyCounter();
	debug(15, 3, "neighborsUdpPing: key = '%s'\n", entry->key);
	debug(15, 3, "neighborsUdpPing: reqnum = %d\n", reqnum);

	if (e->icp_port == echo_port) {
	    debug(15, 4, "neighborsUdpPing: Sending DECHO to dumb cache\n");
	    echo_hdr.reqnum = reqnum;
	    icpUdpSend(theOutIcpConnection,
		url,
		reqnum,
		&e->in_addr,
		0,
		ICP_OP_DECHO,
		LOG_TAG_NONE);
	} else {
	    flags = 0;
	    /* check if we should set ICP_FLAG_HIT_OBJ */
	    if (opt_udp_hit_obj)
		if (!BIT_TEST(proto->request->flags, REQ_NOCACHE))
		    if (e->icp_version == ICP_VERSION_2)
			flags |= ICP_FLAG_HIT_OBJ;
	    icpUdpSend(theOutIcpConnection,
		url,
		reqnum,
		&e->in_addr,
		flags,
		ICP_OP_QUERY,
		LOG_TAG_NONE);
	}

	e->stats.ack_deficit++;
	e->stats.pings_sent++;
	if (e->neighbor_up && e->stats.ack_deficit >= HIER_MAX_DEFICIT) {
	    debug(15, 0, "neighborsUdpPing: Detected DEAD %s: %s\n",
		e->type == EDGE_SIBLING ? "SIBLING" : "PARENT",
		e->host);
	    e->neighbor_up = 0;
	}
#ifdef USE_MULTICAST
	if (e->ttl == 0)
#endif
	    if (e->neighbor_up)
		mem->e_pings_n_pings++;		/* expect a reply from it */
	friends->first_ping = e->next;
    }

    /* only do source_ping if we have neighbors */
    if (friends->n) {
	if (!proto->source_ping) {
	    debug(15, 6, "neighborsUdpPing: Source Ping is disabled.\n");
	} else if ((hep = ipcache_gethostbyname(host, IP_BLOCKING_LOOKUP))) {
	    debug(15, 6, "neighborsUdpPing: Source Ping: to %s for '%s'\n",
		host, url);
	    to_addr.sin_family = AF_INET;
	    xmemcpy(&to_addr.sin_addr, hep->h_addr, hep->h_length);
	    to_addr.sin_port = htons(echo_port);
	    echo_hdr.reqnum = reqnum;
	    icpUdpSend(theOutIcpConnection,
		url,
		reqnum,
		&to_addr,
		0,
		ICP_OP_SECHO,
		LOG_TAG_NONE);
	} else {
	    debug(15, 6, "neighborsUdpPing: Source Ping: unknown host: %s\n",
		host);
	}
    }
    return (mem->e_pings_n_pings);
}

static void neighborAlive(edge * e, const MemObject * mem, const icp_common_t * header)
{
    int rtt;
    int n;
    /* Neighbor is alive, reset the ack deficit */
    if (e->stats.ack_deficit >= HIER_MAX_DEFICIT) {
	debug(15, 0, "Detected REVIVED %s: %s/%d/%d\n",
	    e->type == EDGE_SIBLING ? "SIBLING" : "PARENT",
	    e->host, e->http_port, e->icp_port);
    }
    e->neighbor_up = 1;
    e->stats.ack_deficit = 0;
    n = ++e->stats.pings_acked;
    if ((icp_opcode) header->opcode <= ICP_OP_END)
	e->stats.counts[header->opcode]++;
    if (n > RTT_AV_FACTOR)
	n = RTT_AV_FACTOR;
    if (mem) {
	rtt = tvSubMsec(mem->start_ping, current_time);
	e->stats.rtt = (e->stats.rtt * (n - 1) + rtt) / n;
	e->icp_version = (int) header->version;
    }
}

/* I should attach these records to the entry.  We take the first
 * hit we get our wait until everyone misses.  The timeout handler
 * call needs to nip this shopping list or call one of the misses.
 * 
 * If a hit process is already started, then sobeit
 */
void neighborsUdpAck(fd, url, header, from, entry, data, data_sz)
     int fd;
     char *url;
     icp_common_t *header;
     struct sockaddr_in *from;
     StoreEntry *entry;
     char *data;
     int data_sz;
{
    edge *e = NULL;
    MemObject *mem = entry->mem_obj;
    int w_rtt;
    HttpStateData *httpState = NULL;

    debug(15, 6, "neighborsUdpAck: opcode %d '%s'\n",
	(int) header->opcode, url);
    if ((e = whichEdge(from)))
	neighborAlive(e, mem, header);
    if (header->opcode > ICP_OP_END)
	return;
    /* check if someone is already fetching it */
    if (BIT_TEST(entry->flag, ENTRY_DISPATCHED)) {
	debug(15, 3, "neighborsUdpAck: '%s' already being fetched.\n", url);
	return;
    }
    if (mem == NULL) {
	debug(15, 1, "Ignoring ICP reply for missing mem_obj: %s\n", url);
	return;
    }
    if (entry->ping_status != PING_WAITING) {
	debug(15, 1, "neighborsUdpAck: '%s' unexpected ICP reply.\n", url);
	return;
    }
    if (entry->lock_count == 0) {
	debug(12, 1, "neighborsUdpAck: '%s' has no locks\n", url);
	return;
    }
    debug(15, 3, "neighborsUdpAck: %s for '%s' from %s %s \n",
	IcpOpcodeStr[header->opcode], url,
	e ? e->type == EDGE_PARENT ? "PARENT" : "SIBLING" : "UNKNOWN",
	e ? e->host : "source");
    mem->e_pings_n_acks++;

    if (header->opcode == ICP_OP_SECHO) {
	/* Received source-ping reply */
	if (e) {
	    debug(15, 1, "Ignoring SECHO from neighbor %s\n", e->host);
	} else {
	    /* if we reach here, source-ping reply is the first 'parent',
	     * so fetch directly from the source */
	    debug(15, 6, "Source is the first to respond.\n");
	    hierarchy_log_append(entry,
		HIER_SOURCE_FASTEST,
		0,
		inet_ntoa(from->sin_addr));
	    entry->ping_status = PING_DONE;
	    getFromCache(0, entry, NULL, entry->mem_obj->request);
	    return;
	}
    } else if (header->opcode == ICP_OP_HIT_OBJ) {
	if (entry->object_len != 0) {
	    debug(15, 1, "Too late UDP_HIT_OBJ '%s'?\n", entry->url);
	} else {
	    protoCancelTimeout(0, entry);
	    entry->ping_status = PING_DONE;
	    httpState = xcalloc(1, sizeof(HttpStateData));
	    httpState->entry = entry;
	    httpProcessReplyHeader(httpState, data, data_sz);
	    storeAppend(entry, data, data_sz);
	    storeComplete(entry);
	    hierarchy_log_append(entry,
		HIER_UDP_HIT_OBJ,
		0,
		e ? e->host : inet_ntoa(from->sin_addr));
	    if (httpState->reply_hdr)
		put_free_8k_page(httpState->reply_hdr);
	    safe_free(httpState);
	    return;
	}
    } else if (header->opcode == ICP_OP_HIT) {
	if (e == NULL) {
	    debug(15, 1, "Ignoring HIT from non-neighbor %s\n",
		inet_ntoa(from->sin_addr));
	} else {
	    hierarchy_log_append(entry,
		e->type == EDGE_SIBLING ? HIER_NEIGHBOR_HIT : HIER_PARENT_HIT,
		0,
		e->host);
	    entry->ping_status = PING_DONE;
	    getFromCache(0, entry, e, entry->mem_obj->request);
	    return;
	}
    } else if (header->opcode == ICP_OP_DECHO) {
	if (e == NULL) {
	    debug(15, 1, "Ignoring DECHO from non-neighbor %s\n",
		inet_ntoa(from->sin_addr));
	} else if (e->type == EDGE_SIBLING) {
	    fatal_dump("neighborsUdpAck: Found non-ICP cache as SIBLING\n");
	} else {
	    w_rtt = tvSubMsec(mem->start_ping, current_time) / e->weight;
	    if (mem->w_rtt == 0 || w_rtt < mem->w_rtt) {
		mem->e_pings_first_miss = e;
		mem->w_rtt = w_rtt;
	    }
	}
    } else if (header->opcode == ICP_OP_MISS) {
	if (e == NULL) {
	    debug(15, 1, "Ignoring MISS from non-neighbor %s\n",
		inet_ntoa(from->sin_addr));
	} else if (e->type == EDGE_PARENT) {
	    w_rtt = tvSubMsec(mem->start_ping, current_time) / e->weight;
	    if (mem->w_rtt == 0 || w_rtt < mem->w_rtt) {
		mem->e_pings_first_miss = e;
		mem->w_rtt = w_rtt;
	    }
	}
    } else if (header->opcode == ICP_OP_DENIED) {
	if (e == NULL) {
	    debug(15, 1, "Ignoring DENIED from non-neighbor %s\n",
		inet_ntoa(from->sin_addr));
	} else if (e->stats.pings_acked > 100) {
	    if (100 * e->stats.counts[ICP_OP_DENIED] / e->stats.pings_acked > 95) {
		debug(15, 0, "95%% of replies from '%s' are UDP_DENIED\n", e->host);
		debug(15, 0, "Disabling '%s', please check your configuration.\n", e->host);
		neighborRemove(e);
	    }
	}
    } else if (header->opcode == ICP_OP_RELOADING) {
	if (e)
	    debug(15, 3, "neighborsUdpAck: %s is RELOADING\n", e->host);
    } else {
	debug(15, 0, "neighborsUdpAck: Unexpected ICP reply: %s\n",
	    IcpOpcodeStr[header->opcode]);
    }
    if (mem->e_pings_n_acks == mem->e_pings_n_pings) {
	entry->ping_status = PING_DONE;
	debug(15, 6, "neighborsUdpAck: All replies received.\n");
	/* pass in fd=0 here so getFromCache() looks up the real FD
	 * and resets the timeout handler */
	getFromDefaultSource(0, entry);
    }
}

#ifndef USE_MULTICAST
void neighbors_cf_add(host, type, http_port, icp_port, options, weight)
#else
void neighbors_cf_add(host, type, http_port, icp_port, options, weight,
    mcast_ttl)
#endif				/* USE_MULTICAST */
     char *host;
     char *type;
     int http_port;
     int icp_port;
     int options;
     int weight;
#ifdef USE_MULTICAST
     int mcast_ttl;
#endif /* USE_MULTICAST */
{
    struct neighbor_cf *t, *u;

    t = xcalloc(1, sizeof(struct neighbor_cf));
    t->host = xstrdup(host);
    t->type = xstrdup(type);
    t->http_port = http_port;
    t->icp_port = icp_port;
    t->options = options;
    t->weight = weight;
#ifdef USE_MULTICAST
    t->mcast_ttl = mcast_ttl;
#endif /* USE_MULTICAST */
    t->next = (struct neighbor_cf *) NULL;

    if (Neighbor_cf == (struct neighbor_cf *) NULL) {
	Neighbor_cf = t;
    } else {
	for (u = Neighbor_cf; u->next; u = u->next);
	u->next = t;
    }
}

void neighbors_cf_domain(host, domain)
     char *host;
     char *domain;
{
    struct neighbor_cf *t = NULL;
    dom_list *l = NULL;
    dom_list **L = NULL;

    for (t = Neighbor_cf; t; t = t->next) {
	if (strcmp(t->host, host) == 0)
	    break;
    }
    if (t == NULL) {
	debug(15, 0, "%s, line %d: No cache_host '%s'\n",
	    cfg_filename, config_lineno, host);
	return;
    }
    l = xmalloc(sizeof(dom_list));
    l->do_ping = 1;
    if (*domain == '!') {	/* check for !.edu */
	l->do_ping = 0;
	domain++;
    }
    l->domain = xstrdup(domain);
    l->next = NULL;
    for (L = &(t->domains); *L; L = &((*L)->next));
    *L = l;
}

void neighbors_cf_acl(host, aclname)
     char *host;
     char *aclname;
{
    struct neighbor_cf *t = NULL;
    struct _acl_list *L = NULL;
    struct _acl_list **Tail = NULL;
    struct _acl *a = NULL;

    for (t = Neighbor_cf; t; t = t->next) {
	if (strcmp(t->host, host) == 0)
	    break;
    }
    if (t == NULL) {
	debug(15, 0, "%s, line %d: No cache_host '%s'\n",
	    cfg_filename, config_lineno, host);
	return;
    }
    L = xcalloc(1, sizeof(struct _acl_list));
    L->op = 1;
    if (*aclname == '!') {
	L->op = 0;
	aclname++;
    }
    debug(15, 3, "neighbors_cf_acl: looking for ACL name '%s'\n", aclname);
    a = aclFindByName(aclname);
    if (a == NULL) {
	debug(15, 0, "%s line %d: %s\n",
	    cfg_filename, config_lineno, config_input_line);
	debug(15, 0, "neighbors_cf_acl: ACL name '%s' not found.\n", aclname);
	xfree(L);
	return;
    }
    if (a->type == ACL_SRC_IP) {
	debug(15, 0, "%s line %d: %s\n",
	    cfg_filename, config_lineno, config_input_line);
	debug(15, 0, "neighbors_cf_acl: 'src' ALC's not supported for 'cache_host_acl'\n");
	xfree(L);
	return;
    }
    L->acl = a;
    for (Tail = &(t->acls); *Tail; Tail = &((*Tail)->next));
    *Tail = L;
}

void neighbors_init()
{
    struct neighbor_cf *t = NULL;
    struct neighbor_cf *next = NULL;
    char *me = getMyHostname();
    edge *e = NULL;
    char *fname = NULL;

    debug(15, 1, "neighbors_init: Initializing Neighbors...\n");

    if (friends == NULL)
	friends = xcalloc(1, sizeof(neighbors));

    if ((fname = getHierarchyLogFile()))
	neighborsOpenLog(fname);

    for (t = Neighbor_cf; t; t = next) {
	next = t->next;
	if (!strcmp(t->host, me) && t->http_port == getHttpPortNum()) {
	    debug(15, 0, "neighbors_init: skipping cache_host %s %s %d %d\n",
		t->type, t->host, t->http_port, t->icp_port);
	    continue;
	}
	debug(15, 1, "Adding a %s: %s/%d/%d\n",
	    t->type, t->host, t->http_port, t->icp_port);

	e = xcalloc(1, sizeof(edge));
	e->http_port = t->http_port;
	e->icp_port = t->icp_port;
#ifdef USE_MULTICAST
	e->mcast_ttl = t->mcast_ttl;
#endif /* USE_MULTICAST */
	e->options = t->options;
	e->weight = t->weight;
	e->host = t->host;
	e->domains = t->domains;
	e->acls = t->acls;
	e->neighbor_up = 1;
	e->icp_version = ICP_VERSION_CURRENT;
	if (!strcmp(t->type, "parent")) {
	    friends->n_parent++;
	    e->type = EDGE_PARENT;
	} else {
	    friends->n_neighbor++;
	    e->type = EDGE_SIBLING;
	}
	safe_free(t->type);

	/* Append edge */
	if (!friends->edges_head)
	    friends->edges_head = e;
	if (friends->edges_tail)
	    friends->edges_tail->next = e;
	friends->edges_tail = e;
	friends->n++;

	safe_free(t);
    }
    Neighbor_cf = NULL;
    any_addr.s_addr = inet_addr("0.0.0.0");
}

void neighbors_rotate_log()
{
    char *fname = NULL;
    int i;
    static char from[MAXPATHLEN];
    static char to[MAXPATHLEN];

    if ((fname = getHierarchyLogFile()) == NULL)
	return;

    debug(15, 1, "neighbors_rotate_log: Rotating.\n");

    /* Rotate numbers 0 through N up one */
    for (i = getLogfileRotateNumber(); i > 1;) {
	i--;
	sprintf(from, "%s.%d", fname, i - 1);
	sprintf(to, "%s.%d", fname, i);
	rename(from, to);
    }
    /* Rotate the current log to .0 */
    if (getLogfileRotateNumber() > 0) {
	sprintf(to, "%s.%d", fname, 0);
	rename(fname, to);
    }
    neighborsOpenLog(fname);
}

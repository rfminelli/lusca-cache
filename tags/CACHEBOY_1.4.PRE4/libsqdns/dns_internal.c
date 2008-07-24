
/*
 * $Id$
 *
 * DEBUG: section 78    DNS lookups; interacts with lib/rfc1035.c
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

#include "../include/config.h"
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
 
#include "../include/Array.h"
#include "../include/Stack.h"
#include "../include/util.h"
#include "../include/hash.h"
#include "../include/rfc1035.h"
#include "../libcore/valgrind.h"
#include "../libcore/varargs.h"
#include "../libcore/debug.h"
#include "../libcore/kb.h"
#include "../libcore/gb.h"
#include "../libcore/tools.h"
#include "../libcore/dlink.h"
 
#include "../libmem/MemPool.h"
#include "../libmem/MemBufs.h"
#include "../libmem/MemBuf.h"
#include "../libmem/wordlist.h"
 
#include "../libcb/cbdata.h"
  
#include "../libiapp/iapp_ssl.h"
#include "../libiapp/comm.h"
#include "../libiapp/event.h"
#include "../libiapp/pconn_hist.h"
   
#if defined(_SQUID_CYGWIN_)
#include <sys/ioctl.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#include "dns.h"
#include "dns_internal.h"

/* MS VisualStudio Projects are monolithic, so we need the following
 * #ifndef to exclude the internal DNS code from compile process when
 * using External DNS process.
 */
#if !USE_DNSSERVERS

#if HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#if HAVE_RESOLV_H
#include <resolv.h>
#endif

#ifdef _SQUID_WIN32_
#include <windows.h>
#endif

int RcodeMatrix[MAX_RCODE][MAX_ATTEMPT];

CBDATA_TYPE(idns_query);

ns *nameservers = NULL;
sp *searchpath = NULL;
int nns = 0;
static int nns_alloc = 0;
int npc = 0;
static int npc_alloc = 0;
dlink_list idns_lru_list;
static int event_queued = 0;
static hash_table *idns_lookup_hash = NULL;
static int DnsSocket = -1;

DnsConfigStruct DnsConfig;

static void idnsCacheQuery(idns_query * q);
static void idnsSendQuery(idns_query * q);
static int idnsFromKnownNameserver(struct sockaddr_in *from);
static idns_query *idnsFindQuery(unsigned short id);
static void idnsGrokReply(const char *buf, size_t sz);
static PF idnsRead;
static EVH idnsCheckQueue;
static void idnsTickleQueue(void);
static void idnsRcodeCount(int, int);

void
idnsAddNameserver(const char *buf)
{
    struct in_addr A;
    if (!safe_inet_addr(buf, &A)) {
	debug(78, 0) ("WARNING: rejecting '%s' as a name server, because it is not a numeric IP address\n", buf);
	return;
    }
    if (A.s_addr == 0) {
	debug(78, 0) ("WARNING: Squid does not accept 0.0.0.0 in DNS server specifications.\n");
	debug(78, 0) ("Will be using 127.0.0.1 instead, assuming you meant that DNS is running on the same machine\n");
	safe_inet_addr("127.0.0.1", &A);
    }
    if (nns == nns_alloc) {
	int oldalloc = nns_alloc;
	ns *oldptr = nameservers;
	if (nns_alloc == 0)
	    nns_alloc = 2;
	else
	    nns_alloc <<= 1;
	nameservers = xcalloc(nns_alloc, sizeof(*nameservers));
	if (oldptr && oldalloc)
	    xmemcpy(nameservers, oldptr, oldalloc * sizeof(*nameservers));
	if (oldptr)
	    safe_free(oldptr);
    }
    assert(nns < nns_alloc);
    nameservers[nns].S.sin_family = AF_INET;
    nameservers[nns].S.sin_port = htons(NS_DEFAULTPORT);
    nameservers[nns].S.sin_addr.s_addr = A.s_addr;
    debug(78, 3) ("idnsAddNameserver: Added nameserver #%d: %s\n",
	nns, inet_ntoa(nameservers[nns].S.sin_addr));
    nns++;
}

void
idnsAddPathComponent(const char *buf)
{
    if (npc == npc_alloc) {
	int oldalloc = npc_alloc;
	sp *oldptr = searchpath;
	if (0 == npc_alloc)
	    npc_alloc = 2;
	else
	    npc_alloc <<= 1;
	searchpath = (sp *) xcalloc(npc_alloc, sizeof(*searchpath));
	if (oldptr && oldalloc)
	    xmemcpy(searchpath, oldptr, oldalloc * sizeof(*searchpath));
	if (oldptr)
	    safe_free(oldptr);
    }
    assert(npc < npc_alloc);
    strcpy(searchpath[npc].domain, buf);
    debug(78, 3) ("idnsAddPathComponent: Added domain #%d: %s\n",
	npc, searchpath[npc].domain);
    npc++;
}


void
idnsFreeNameservers(void)
{
    safe_free(nameservers);
    nns = nns_alloc = 0;
}

void
idnsFreeSearchpath(void)
{
    safe_free(searchpath);
    npc = npc_alloc = 0;
}


static void
idnsTickleQueue(void)
{
    if (event_queued)
	return;
    if (NULL == idns_lru_list.tail)
	return;
    eventAdd("idnsCheckQueue", idnsCheckQueue, NULL, 1.0, 1);
    event_queued = 1;
}

static void
idnsTcpCleanup(idns_query * q)
{
    if (q->tcp_socket != -1) {
	comm_close(q->tcp_socket);
	q->tcp_socket = -1;
    }
    if (q->tcp_buffer) {
	memFreeBuf(q->tcp_buffer_size, q->tcp_buffer);
	q->tcp_buffer = NULL;
    }
}

static void
idnsSendQuery(idns_query * q)
{
    int x;
    int ns;
    if (DnsSocket < 0) {
	debug(78, 1) ("idnsSendQuery: Can't send query, no DNS socket!\n");
	return;
    }
    /* XXX Select nameserver */
    assert(nns > 0);
    assert(q->lru.next == NULL);
    assert(q->lru.prev == NULL);
    idnsTcpCleanup(q);
  try_again:
    ns = q->nsends % nns;
    x = comm_udp_sendto(DnsSocket,
	&nameservers[ns].S,
	sizeof(nameservers[ns].S),
	q->buf,
	q->sz);
    q->nsends++;
    q->queue_t = q->sent_t = current_time;
    if (x < 0) {
	debug(50, 1) ("idnsSendQuery: FD %d: sendto: %s\n",
	    DnsSocket, xstrerror());
	if (q->nsends % nns != 0)
	    goto try_again;
    } else {
	fd_bytes(DnsSocket, x, FD_WRITE);
	commSetSelect(DnsSocket, COMM_SELECT_READ, idnsRead, NULL, 0);
    }
    nameservers[ns].nqueries++;
    dlinkAdd(q, &q->lru, &idns_lru_list);
    idnsTickleQueue();
}

static int
idnsFromKnownNameserver(struct sockaddr_in *from)
{
    int i;
    for (i = 0; i < nns; i++) {
	if (nameservers[i].S.sin_addr.s_addr != from->sin_addr.s_addr)
	    continue;
	if (nameservers[i].S.sin_port != from->sin_port)
	    continue;
	return i;
    }
    return -1;
}

static idns_query *
idnsFindQuery(unsigned short id)
{
    dlink_node *n;
    idns_query *q;
    for (n = idns_lru_list.tail; n; n = n->prev) {
	q = n->data;
	if (q->id == id)
	    return q;
    }
    return NULL;
}

static unsigned short
idnsQueryID(void)
{
    unsigned short id = squid_random() & 0xFFFF;
    unsigned short first_id = id;

    while (idnsFindQuery(id)) {
	id++;

	if (id == first_id) {
	    debug(78, 1) ("idnsQueryID: Warning, too many pending DNS requests\n");
	    break;
	}
    }

    return id;
}


static void
idnsCallback(idns_query * q, rfc1035_rr * answers, int n, const char *error)
{
    int valid;
    valid = cbdataValid(q->callback_data);
    cbdataUnlock(q->callback_data);
    if (valid)
	q->callback(q->callback_data, answers, n, error);
    while (q->queue) {
	idns_query *q2 = q->queue;
	q->queue = q2->queue;
	valid = cbdataValid(q2->callback_data);
	cbdataUnlock(q2->callback_data);
	if (valid)
	    q2->callback(q2->callback_data, answers, n, error);
	cbdataFree(q2);
    }
    if (q->hash.key) {
	hash_remove_link(idns_lookup_hash, &q->hash);
	q->hash.key = NULL;
    }
}

static void
idnsReadTcp(int fd, void *data)
{
    ssize_t n;
    idns_query *q = data;
    int ns = (q->nsends - 1) % nns;
    if (!q->tcp_buffer)
	q->tcp_buffer = memAllocBuf(1024, &q->tcp_buffer_size);
    CommStats.syscalls.sock.reads++;
    n = FD_READ_METHOD(q->tcp_socket, q->tcp_buffer + q->tcp_buffer_offset, q->tcp_buffer_size - q->tcp_buffer_offset);
    if (n < 0 && ignoreErrno(errno)) {
	commSetSelect(q->tcp_socket, COMM_SELECT_READ, idnsReadTcp, q, 0);
	return;
    }
    if (n <= 0) {
	debug(78, 1) ("idnsReadTcp: Short response from nameserver %d for %s.\n", ns + 1, q->name);
	idnsTcpCleanup(q);
	return;
    }
    fd_bytes(fd, n, FD_READ);
    q->tcp_buffer_offset += n;
    if (q->tcp_buffer_offset > 2) {
	unsigned short response_size = ntohs(*(short *) q->tcp_buffer);
	if (q->tcp_buffer_offset >= response_size + 2) {
	    nameservers[ns].nreplies++;
	    idnsGrokReply(q->tcp_buffer + 2, response_size);
	    return;
	}
	if (q->tcp_buffer_size < response_size + 2)
	    q->tcp_buffer = memReallocBuf(q->tcp_buffer, response_size + 2, &q->tcp_buffer_size);
    }
    commSetSelect(q->tcp_socket, COMM_SELECT_READ, idnsReadTcp, q, 0);
}

static void
idnsSendTcpQueryDone(int fd, char *bufnotused, size_t size, int errflag, void *data)
{
    idns_query *q = data;
    if (size > 0)
	fd_bytes(fd, size, FD_WRITE);
    if (errflag == COMM_ERR_CLOSING)
	return;
    if (errflag) {
	idnsTcpCleanup(q);
	return;
    }
    commSetSelect(q->tcp_socket, COMM_SELECT_READ, idnsReadTcp, q, 0);
}

static void
idnsSendTcpQuery(int fd, int status, void *data)
{
    MemBuf buf;
    idns_query *q = data;
    short nsz;
    if (status != COMM_OK) {
	int ns = (q->nsends - 1) % nns;
	debug(78, 1) ("idnsSendTcpQuery: Failed to connect to DNS server %d using TCP\n", ns + 1);
	idnsTcpCleanup(q);
	return;
    }
    memBufInit(&buf, q->sz + 2, q->sz + 2);
    nsz = htons(q->sz);
    memBufAppend(&buf, &nsz, 2);
    memBufAppend(&buf, q->buf, q->sz);
    comm_write_mbuf(q->tcp_socket, buf, idnsSendTcpQueryDone, q);
}

static void
idnsRetryTcp(idns_query * q)
{
    struct in_addr addr;
    int ns = (q->nsends - 1) % nns;
    idnsTcpCleanup(q);
    if (DnsConfig.udp_outgoing.s_addr != no_addr.s_addr)
	addr = DnsConfig.udp_outgoing;
    else
	addr = DnsConfig.udp_incoming;
    q->tcp_socket = comm_open(SOCK_STREAM,
	IPPROTO_TCP,
	addr,
	0,
	COMM_NONBLOCKING,
	"DNS TCP Socket");
    q->queue_t = q->sent_t = current_time;
    dlinkAdd(q, &q->lru, &idns_lru_list);
    commConnectStart(q->tcp_socket,
	inet_ntoa(nameservers[ns].S.sin_addr),
	ntohs(nameservers[ns].S.sin_port),
	idnsSendTcpQuery,
	q,
	NULL
	);
}

static void
idnsGrokReply(const char *buf, size_t sz)
{
    int n;
    rfc1035_message *message = NULL;
    idns_query *q;
    n = rfc1035MessageUnpack(buf,
	sz,
	&message);
    if (message == NULL) {
	debug(78, 2) ("idnsGrokReply: Malformed DNS response\n");
	return;
    }
    debug(78, 3) ("idnsGrokReply: ID %#hx, %d answers\n", message->id, n);

    q = idnsFindQuery(message->id);

    if (q == NULL) {
	debug(78, 3) ("idnsGrokReply: Late response\n");
	rfc1035MessageDestroy(message);
	return;
    }
    if (rfc1035QueryCompare(&q->query, message->query) != 0) {
	debug(78, 3) ("idnsGrokReply: Query mismatch (%s != %s)\n", q->query.name, message->query->name);
	rfc1035MessageDestroy(message);
	return;
    }
    dlinkDelete(&q->lru, &idns_lru_list);
    if (message->tc && q->tcp_socket == -1) {
	debug(78, 2) ("idnsGrokReply: Response for %s truncated. Retrying using TCP\n", message->query->name);
	rfc1035MessageDestroy(message);
	idnsRetryTcp(q);
	return;
    }
    idnsRcodeCount(n, q->attempt);
    q->error = NULL;
    if (n < 0) {
	debug(78, 3) ("idnsGrokReply: error %s (%d)\n", rfc1035_error_message, rfc1035_errno);
	q->error = rfc1035_error_message;
	q->rcode = -n;
	if (q->rcode == 2 && ++q->attempt < MAX_ATTEMPT) {
	    /*
	     * RCODE 2 is "Server failure - The name server was
	     * unable to process this query due to a problem with
	     * the name server."
	     */
	    rfc1035MessageDestroy(message);
	    q->start_t = current_time;
	    q->id = idnsQueryID();
	    rfc1035SetQueryID(q->buf, q->id);
	    idnsSendQuery(q);
	    return;
	}
	if (q->rcode == 3 && q->do_searchpath && q->attempt < MAX_ATTEMPT) {
	    strcpy(q->name, q->orig);
	    if (q->domain < npc) {
		strcat(q->name, ".");
		strcat(q->name, searchpath[q->domain].domain);
		debug(78, 3) ("idnsGrokReply: searchpath used for %s\n",
		    q->name);
		q->domain++;
	    } else {
		q->attempt++;
	    }
	    rfc1035MessageDestroy(message);
	    if (q->hash.key) {
		hash_remove_link(idns_lookup_hash, &q->hash);
		q->hash.key = NULL;
	    }
	    q->start_t = current_time;
	    q->id = idnsQueryID();
	    rfc1035SetQueryID(q->buf, q->id);
	    q->sz = rfc1035BuildAQuery(q->name, q->buf, sizeof(q->buf), q->id,
		&q->query);

	    idnsCacheQuery(q);
	    idnsSendQuery(q);
	    return;
	}
    }
    idnsCallback(q, message->answer, n, q->error);
    rfc1035MessageDestroy(message);

    idnsTcpCleanup(q);
    cbdataFree(q);
}

static void
idnsRead(int fd, void *data)
{
    ssize_t len;
    struct sockaddr_in from;
    socklen_t from_len;
    int max = INCOMING_DNS_MAX;
    static char rbuf[SQUID_UDP_SO_RCVBUF];
    int ns;
    while (max--) {
	from_len = sizeof(from);
	memset(&from, '\0', from_len);
	CommStats.syscalls.sock.recvfroms++;
	len = recvfrom(fd, rbuf, sizeof(rbuf), 0, (struct sockaddr *) &from, &from_len);
	if (len == 0)
	    break;
	if (len < 0) {
	    if (ignoreErrno(errno))
		break;
#ifdef _SQUID_LINUX_
	    /* Some Linux systems seem to set the FD for reading and then
	     * return ECONNREFUSED when sendto() fails and generates an ICMP
	     * port unreachable message. */
	    /* or maybe an EHOSTUNREACH "No route to host" message */
	    if (errno != ECONNREFUSED && errno != EHOSTUNREACH)
#endif
		debug(50, 1) ("idnsRead: FD %d recvfrom: %s\n",
		    fd, xstrerror());
	    break;
	}
	fd_bytes(DnsSocket, len, FD_READ);
	debug(78, 3) ("idnsRead: FD %d: received %d bytes from %s.\n",
	    fd,
	    (int) len,
	    inet_ntoa(from.sin_addr));
	ns = idnsFromKnownNameserver(&from);
	if (ns >= 0) {
	    nameservers[ns].nreplies++;
	} else if (DnsConfig.ignore_unknown_nameservers) {
	    static time_t last_warning = 0;
	    if (squid_curtime - last_warning > 60) {
		debug(78, 1) ("WARNING: Reply from unknown nameserver [%s]\n",
		    inet_ntoa(from.sin_addr));
		last_warning = squid_curtime;
	    }
	    continue;
	}
	idnsGrokReply(rbuf, len);
    }
    if (idns_lru_list.head)
	commSetSelect(DnsSocket, COMM_SELECT_READ, idnsRead, NULL, 0);
}

static void
idnsCheckQueue(void *unused)
{
    dlink_node *n;
    dlink_node *p = NULL;
    idns_query *q;
    event_queued = 0;
    if (0 == nns)
	/* name servers went away; reconfiguring or shutting down */
	return;
    for (n = idns_lru_list.tail; n; n = p) {
	p = n->prev;
	q = n->data;
	/* Anything to process in the queue? */
	if (tvSubDsec(q->queue_t, current_time) < DnsConfig.idns_retransmit)
	    break;
	/* Query timer expired? */
	if (tvSubDsec(q->sent_t, current_time) < DnsConfig.idns_retransmit * 1 << ((q->nsends - 1) / nns)) {
	    dlinkDelete(&q->lru, &idns_lru_list);
	    q->queue_t = current_time;
	    dlinkAdd(q, &q->lru, &idns_lru_list);
	    continue;
	}
	debug(78, 3) ("idnsCheckQueue: ID %#04x timeout\n",
	    q->id);
	dlinkDelete(&q->lru, &idns_lru_list);
	if (tvSubDsec(q->start_t, current_time) < DnsConfig.idns_query) {
	    idnsSendQuery(q);
	} else {
	    debug(78, 2) ("idnsCheckQueue: ID %x: giving up after %d tries and %5.1f seconds\n",
		(int) q->id, q->nsends,
		tvSubDsec(q->start_t, current_time));
	    if (q->rcode != 0)
		idnsCallback(q, NULL, -q->rcode, q->error);
	    else
		idnsCallback(q, NULL, -16, "Timeout");
	    idnsTcpCleanup(q);
	    cbdataFree(q);
	}
    }
    idnsTickleQueue();
}

/*
 * rcode < 0 indicates an error, rocde >= 0 indicates success
 */
static void
idnsRcodeCount(int rcode, int attempt)
{
    if (rcode > 0)
	rcode = 0;
    else if (rcode < 0)
	rcode = -rcode;
    if (rcode < MAX_RCODE)
	if (attempt < MAX_ATTEMPT)
	    RcodeMatrix[rcode][attempt]++;
}

/* ====================================================================== */

void
idnsConfigure(struct in_addr incoming_addr, struct in_addr outgoing_addr,
    int ignore_unknown_nameservers, int idns_retransmit,
    int idns_query, int res_defnames)
{
	DnsConfig.udp_incoming = incoming_addr;
	DnsConfig.udp_outgoing = outgoing_addr;
	DnsConfig.ignore_unknown_nameservers = ignore_unknown_nameservers;
	DnsConfig.idns_retransmit = idns_retransmit;
	DnsConfig.idns_query = idns_query;
	DnsConfig.res_defnames = res_defnames;
}


void
idnsInit(void)
{
    static int init = 0;
    CBDATA_INIT_TYPE(idns_query);
    if (DnsSocket < 0) {
	int port;
	struct in_addr addr;
	if (DnsConfig.udp_outgoing.s_addr != no_addr.s_addr)
	    addr = DnsConfig.udp_outgoing;
	else
	    addr = DnsConfig.udp_incoming;
	DnsSocket = comm_open(SOCK_DGRAM,
	    IPPROTO_UDP,
	    addr,
	    0,
	    COMM_NONBLOCKING,
	    "DNS Socket");
	if (DnsSocket < 0)
	    libcore_fatalf("Could not create a DNS socket: errno %d", errno);
	/* Ouch... we can't call functions using debug from a debug
	 * statement. Doing so messes up the internal _db_level
	 */
	port = comm_local_port(DnsSocket);
	debug(78, 1) ("DNS Socket created at %s, port %d, FD %d\n",
	    inet_ntoa(addr),
	    port, DnsSocket);
    }
    assert(0 == nns);
    if (!init) {
	memset(RcodeMatrix, '\0', sizeof(RcodeMatrix));
	idns_lookup_hash = hash_create((HASHCMP *) strcmp, 103, hash_string);
	init++;
	DnsConfig.ndots = 1;
    }
}

void
idnsShutdown(void)
{
    if (DnsSocket < 0)
	return;
    comm_close(DnsSocket);
    DnsSocket = -1;
    idnsFreeNameservers();
    idnsFreeSearchpath();
}

static int
idnsCachedLookup(const char *key, IDNSCB * callback, void *data)
{
    idns_query *q;
    idns_query *old = hash_lookup(idns_lookup_hash, key);
    if (!old)
	return 0;
    q = cbdataAlloc(idns_query);
    q->tcp_socket = -1;
    q->callback = callback;
    q->callback_data = data;
    cbdataLock(q->callback_data);
    q->queue = old->queue;
    old->queue = q;
    return 1;
}

static void
idnsCacheQuery(idns_query * q)
{
    q->hash.key = q->query.name;
    hash_join(idns_lookup_hash, &q->hash);
}

void
idnsALookup(const char *name, IDNSCB * callback, void *data)
{
    unsigned int i;
    int nd = 0;
    idns_query *q;
    if (idnsCachedLookup(name, callback, data))
	return;
    q = cbdataAlloc(idns_query);
    q->tcp_socket = -1;
    q->id = idnsQueryID();

    for (i = 0; i < strlen(name); i++) {
	if (name[i] == '.') {
	    nd++;
	}
    }

    if (DnsConfig.res_defnames && npc > 0 && name[strlen(name) - 1] != '.') {
	q->do_searchpath = 1;
    } else {
	q->do_searchpath = 0;
    }
    strcpy(q->orig, name);
    strcpy(q->name, q->orig);
    if (q->do_searchpath && nd < DnsConfig.ndots) {
	q->domain = 0;
	strcat(q->name, ".");
	strcat(q->name, searchpath[q->domain].domain);
	debug(78, 3) ("idnsALookup: searchpath used for %s\n",
	    q->name);
    }
    q->sz = rfc1035BuildAQuery(q->name, q->buf, sizeof(q->buf), q->id,
	&q->query);

    if (q->sz < 0) {
	/* problem with query data -- query not sent */
	callback(data, NULL, 0, "Internal error");
	cbdataFree(q);
	return;
    }
    debug(78, 3) ("idnsALookup: buf is %d bytes for %s, id = %#hx\n",
	(int) q->sz, q->name, q->id);
    q->callback = callback;
    q->callback_data = data;
    cbdataLock(q->callback_data);
    q->start_t = current_time;
    idnsCacheQuery(q);
    idnsSendQuery(q);
}

void
idnsPTRLookup(const struct in_addr addr, IDNSCB * callback, void *data)
{
    idns_query *q;
    const char *ip = inet_ntoa(addr);
    q = cbdataAlloc(idns_query);
    q->tcp_socket = -1;
    q->id = idnsQueryID();
    q->sz = rfc1035BuildPTRQuery(addr, q->buf, sizeof(q->buf), q->id, &q->query);
    debug(78, 3) ("idnsPTRLookup: buf is %d bytes for %s, id = %#hx\n",
	(int) q->sz, ip, q->id);
    if (q->sz < 0) {
	/* problem with query data -- query not sent */
	callback(data, NULL, 0, "Internal error");
	cbdataFree(q);
	return;
    }
    if (idnsCachedLookup(q->query.name, callback, data)) {
	cbdataFree(q);
	return;
    }
    q->callback = callback;
    q->callback_data = data;
    cbdataLock(q->callback_data);
    q->start_t = current_time;
    idnsCacheQuery(q);
    idnsSendQuery(q);
}

#endif /* USE_DNSSERVERS */

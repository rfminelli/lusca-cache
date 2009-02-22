#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/util.h"
#include "../include/radix.h"
#include "../libcore/tools.h"
#include "../libmem/intlist.h"
#include "../libsqdebug/debug.h"

#include "../libsqinet/sqinet.h"
#include "../libsqinet/inet_legacy.h"

#include "bgp_packet.h"
#include "bgp_rib.h"
#include "bgp_core.h"

/* BEGIN of definitions for radix tree entries */

/* int in memory with length */
typedef u_char m_int[1 + sizeof(unsigned int)];
#define store_m_int(i, m) \
    (i = htonl(i), m[0] = sizeof(m_int), xmemcpy(m+1, &i, sizeof(unsigned int)))
#define get_m_int(i, m) \
    (xmemcpy(&i, m+1, sizeof(unsigned int)), ntohl(i))

/* END of definitions for radix tree entries */

/*
 * Structure for as number information. it could be simply 
 * an intlist but it's coded as a structure for future
 * enhancements (e.g. expires)
 */
struct _as_info {
    intlist *as_number;
    time_t expires;		/* NOTUSED */
};

typedef struct _as_info as_info;

/* entry into the radix tree */
struct _rtentry {
    struct squid_radix_node e_nodes[2];
    as_info *e_info;
    m_int e_addr;
    m_int e_mask;
};

typedef struct _rtentry rtentry;

static int destroyRadixNode(struct squid_radix_node *rn, void *w);
static void destroyRadixNodeInfo(as_info *);

/* PUBLIC */

int
bgp_rib_match_ip(struct squid_radix_node_head *head, void *data, struct in_addr addr)
{
    unsigned long lh;
    struct squid_radix_node *rn;
    as_info *e;
    m_int m_addr;
    intlist *a = NULL;
    intlist *b = NULL;
    lh = ntohl(addr.s_addr);
    debug(53, 3) ("asnMatchIp: Called for %s.\n", inet_ntoa(addr));

    if (head == NULL)
	return 0;
    if (IsNoAddr(&addr))
	return 0;
    if (IsAnyAddr(&addr))
	return 0;
    store_m_int(lh, m_addr);
    rn = squid_rn_match(m_addr, head);
    if (rn == NULL) {
	debug(53, 3) ("asnMatchIp: Address not in as db.\n");
	return 0;
    }
    debug(53, 3) ("asnMatchIp: Found in db!\n");
    e = ((rtentry *) rn)->e_info;
    assert(e);
    for (a = (intlist *) data; a; a = a->next)
	for (b = e->as_number; b; b = b->next)
	    if (a->i == b->i) {
		debug(53, 5) ("asnMatchIp: Found a match!\n");
		return 1;
	    }
    debug(53, 5) ("asnMatchIp: AS not in as db.\n");
    return 0;
}

/* initialize the radix tree structure */

extern int squid_max_keylen;	/* yuck.. this is in lib/radix.c */

void
bgp_rib_init(struct squid_radix_node_head **head)
{
    squid_max_keylen = 40;
    squid_rn_inithead((void **) head, 8);
}

void
bgp_rib_destroy(struct squid_radix_node_head *head)
{
    squid_rn_walktree(head, destroyRadixNode, head);
    destroyRadixNode((struct squid_radix_node *) 0, (void *) head);
}

void
bgp_rib_clean(struct squid_radix_node_head *head)
{
    squid_rn_walktree(head, destroyRadixNode, head);
}

/* PRIVATE */

/* add a network (addr, mask) to the radix tree, with matching AS
 * number */

int
bgp_rib_add_net(struct squid_radix_node_head *head, char *as_string, int as_number)
{
    rtentry *e;
    struct squid_radix_node *rn;
    char dbg1[32], dbg2[32];
    intlist **Tail = NULL;
    intlist *q = NULL;
    as_info *asinfo = NULL;
    struct in_addr in_a, in_m;
    long mask, addr;
    char *t;
    int bitl;

    t = strchr(as_string, '/');
    if (t == NULL) {
	debug(53, 3) ("asnAddNet: failed, invalid response from whois server.\n");
	return 0;
    }
    *t = '\0';
    addr = inet_addr(as_string);
    bitl = atoi(t + 1);
    if (bitl < 0)
	bitl = 0;
    if (bitl > 32)
	bitl = 32;
    mask = bitl ? 0xfffffffful << (32 - bitl) : 0;

    in_a.s_addr = addr;
    in_m.s_addr = mask;
    xstrncpy(dbg1, inet_ntoa(in_a), 32);
    xstrncpy(dbg2, inet_ntoa(in_m), 32);
    addr = ntohl(addr);
    /*mask = ntohl(mask); */
    debug(53, 3) ("asnAddNet: called for %s/%s\n", dbg1, dbg2);
    e = xmalloc(sizeof(rtentry));
    memset(e, '\0', sizeof(rtentry));
    store_m_int(addr, e->e_addr);
    store_m_int(mask, e->e_mask);
    rn = squid_rn_lookup(e->e_addr, e->e_mask, head);
    if (rn != NULL) {
	asinfo = ((rtentry *) rn)->e_info;
	if (intlistFind(asinfo->as_number, as_number)) {
	    debug(53, 3) ("asnAddNet: Ignoring repeated network '%s/%d' for AS %d\n",
		dbg1, bitl, as_number);
	} else {
	    debug(53, 3) ("asnAddNet: Warning: Found a network with multiple AS numbers!\n");
	    for (Tail = &asinfo->as_number; *Tail; Tail = &(*Tail)->next);
	    q = xcalloc(1, sizeof(intlist));
	    q->i = as_number;
	    *(Tail) = q;
	    e->e_info = asinfo;
	}
    } else {
	q = xcalloc(1, sizeof(intlist));
	q->i = as_number;
	asinfo = xmalloc(sizeof(asinfo));
	asinfo->as_number = q;
	rn = squid_rn_addroute(e->e_addr, e->e_mask, head, e->e_nodes);
	rn = squid_rn_match(e->e_addr, head);
	assert(rn != NULL);
	e->e_info = asinfo;
	if (rn == 0) {		/* assert might expand to nothing */
	    xfree(asinfo);
	    xfree(q);
	    xfree(e);
	    debug(53, 3) ("asnAddNet: Could not add entry.\n");
	    return 0;
	}
    }
    e->e_info = asinfo;
    return 1;
}

static int
destroyRadixNode(struct squid_radix_node *rn, void *w)
{
    struct squid_radix_node_head *rnh = (struct squid_radix_node_head *) w;

    if (rn && !(rn->rn_flags & RNF_ROOT)) {
	rtentry *e = (rtentry *) rn;
	rn = squid_rn_delete(rn->rn_key, rn->rn_mask, rnh);
	if (rn == 0)
	    debug(53, 3) ("destroyRadixNode: internal screwup\n");
	destroyRadixNodeInfo(e->e_info);
	xfree(rn);
    }
    return 1;
}

static void
destroyRadixNodeInfo(as_info * e_info)
{
    intlist *prev = NULL;
    intlist *data = e_info->as_number;
    while (data) {
	prev = data;
	data = data->next;
	xfree(prev);
    }
    xfree(data);
}

static int
mask_len(u_long mask)
{
    int len = 32;
    if (mask == 0)
	return 0;
    while ((mask & 1) == 0) {
	len--;
	mask >>= 1;
    }
    return len;
}

#if 0
static int
printRadixNode(struct squid_radix_node *rn, void *w)
{
    StoreEntry *sentry = w;
    rtentry *e = (rtentry *) rn;
    intlist *q;
    as_info *asinfo;
    struct in_addr addr;
    struct in_addr mask;
    assert(e);
    assert(e->e_info);
    (void) get_m_int(addr.s_addr, e->e_addr);
    (void) get_m_int(mask.s_addr, e->e_mask);
    storeAppendPrintf(sentry, "%15s/%d\t",
	inet_ntoa(addr), mask_len(ntohl(mask.s_addr)));
    asinfo = e->e_info;
    assert(asinfo->as_number);
    for (q = asinfo->as_number; q; q = q->next)
	storeAppendPrintf(sentry, " %d", q->i);
    storeAppendPrintf(sentry, "\n");
    return 0;
}
#endif

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
#include "../libcore/tools.h"
#include "../libmem/intlist.h"
#include "../libsqdebug/debug.h"

#include "../libsqinet/sqinet.h"
#include "../libsqinet/inet_legacy.h"

#include "radix.h"
#include "bgp_packet.h"
#include "bgp_rib.h"
#include "bgp_core.h"

u_short
bgp_rib_getasn(void *data)
{
	bgp_rib_aspath_t *a = data;
	return a->origin_as;
}

static void
bgp_rib_asn_free(radix_node_t *ptr, void *cbdata)
{
	free(ptr->data);
}

int
bgp_rib_match_host(bgp_rib_head_t *head, struct in_addr addr)
{
	prefix_t * p;
	radix_node_t *n;
	bgp_rib_aspath_t *a;

	p = New_Prefix(AF_INET, &addr, 32, NULL);

	n = radix_search_best(head->rh, p);
	if (n == NULL) {
		debug(85, 3) ("bgp_rib_match_host: %s: no match\n", inet_ntoa(addr));
		Deref_Prefix(p);
		return -1;
	}
	debug(85, 3) ("bgp_rib_match_host: %s: match; AS %d\n", inet_ntoa(addr), bgp_rib_getasn(n->data));
	Deref_Prefix(p);
	a = n->data;
	return a->origin_as;
}


int
bgp_rib_match_net(bgp_rib_head_t *head, struct in_addr addr, int masklen)
{
	prefix_t * p;
	radix_node_t *n;
	bgp_rib_aspath_t *a;

	p = New_Prefix(AF_INET, &addr, masklen, NULL);

	n = radix_search_best(head->rh, p);
	if (n == NULL) {
		debug(85, 3) ("bgp_rib_match_net: %s/%d: no match\n", inet_ntoa(addr), masklen);
		Deref_Prefix(p);
		return -1;
	}
	debug(85, 3) ("bgp_rib_match_net: %s/%d: match; AS %d\n", inet_ntoa(addr), masklen, bgp_rib_getasn(n->data));
	Deref_Prefix(p);
	a = n->data;
	return a->origin_as;
}

/* initialize the radix tree structure */

extern int squid_max_keylen;	/* yuck.. this is in lib/radix.c */

void
bgp_rib_init(bgp_rib_head_t *head)
{
	debug(85, 1) ("bgp_rib_init: %p: called\n", head);
	head->rh = New_Radix();
}

void
bgp_rib_destroy(bgp_rib_head_t *head)
{
	debug(85, 1) ("bgp_rib_destroy: %p: called\n", head);
	bzero(head, sizeof(*head));
	
}

void
bgp_rib_clean(bgp_rib_head_t *head)
{
	debug(85, 1) ("bgp_rib_clean: %p: called\n", head);
	//Clear_Radix(head->rh, bgp_rib_asn_free, NULL);
	Destroy_Radix(head->rh, bgp_rib_asn_free, NULL);
	head->rh = New_Radix();
	head->num_prefixes = 0;
}

static void
bgp_rib_clear_node(bgp_rib_head_t *head, radix_node_t *n)
{
	bgp_rib_asn_free(n, NULL);	
	radix_remove(head->rh, n);
	head->num_prefixes--;
}

int
bgp_rib_add_net(bgp_rib_head_t *head, struct in_addr addr, int masklen, u_short origin_as)
{
	prefix_t * p;
	radix_node_t *n;
	bgp_rib_aspath_t *a;

	debug(85, 3) ("bgp_rib_add_net: %s/%d; AS %d\n", inet_ntoa(addr), masklen, origin_as);
	p = New_Prefix(AF_INET, &addr, masklen, NULL);
	n = radix_search_exact(head->rh, p);
	if (n != NULL) {
		debug(85, 3) ("bgp_rib_add_net: %s/%d: removing before re-adding\n", inet_ntoa(addr), masklen);
		bgp_rib_clear_node(head, n);
	}

	n = radix_lookup(head->rh, p);
	/* XXX should verify? */
	/* XXX should add some path data, etc? */
	Deref_Prefix(p);
	a = xcalloc(1, sizeof(bgp_rib_aspath_t));
	a->origin_as = origin_as;
	n->data = a;
	head->num_prefixes++;
	return 1;
}

int
bgp_rib_del_net(bgp_rib_head_t *head, struct in_addr addr, int masklen)
{
	prefix_t * p;
	radix_node_t *n;

	debug(85, 3) ("bgp_rib_del_net: %s/%d\n", inet_ntoa(addr), masklen);

	p = New_Prefix(AF_INET, &addr, masklen, NULL);

	n = radix_search_exact(head->rh, p);
	if (n == NULL) {
		debug(85, 3) ("bgp_rib_del_net: %s/%d: NOT FOUND?!\n", inet_ntoa(addr), masklen);
		Deref_Prefix(p);
		return 0;
	}
	bgp_rib_clear_node(head, n);
	Deref_Prefix(p);
	return 1;
}

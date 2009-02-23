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

int
bgp_rib_match_net(bgp_rib_head_t *head, struct in_addr addr, int masklen)
{
	prefix_t * p;
	radix_node_t *n;

	p = New_Prefix(AF_INET, &addr, masklen, NULL);

	n = radix_search_best(head->rh, p);
	if (n == NULL) {
		debug(85, 1) ("bgp_rib_match_net: %s/%d: no match\n", inet_ntoa(addr), masklen);
		Deref_Prefix(p);
		return 0;
	}
	debug(85, 1) ("bgp_rib_match_net: %s/%d: match\n", inet_ntoa(addr), masklen);
	Deref_Prefix(p);
	return 1;
}

/* initialize the radix tree structure */

extern int squid_max_keylen;	/* yuck.. this is in lib/radix.c */

void
bgp_rib_init(bgp_rib_head_t *head)
{
	head->rh = New_Radix();
}

void
bgp_rib_destroy(bgp_rib_head_t *head)
{
	Destroy_Radix(head->rh, NULL, NULL);
}

void
bgp_rib_clean(bgp_rib_head_t *head)
{
	Clear_Radix(head->rh, NULL, NULL);
}


int
bgp_rib_add_net(bgp_rib_head_t *head, struct in_addr addr, int masklen)
{
	prefix_t * p;
	radix_node_t *n;

	debug(85, 1) ("bgp_rib_add_net: %s/%d\n", inet_ntoa(addr), masklen);
	p = New_Prefix(AF_INET, &addr, masklen, NULL);
	n = radix_search_exact(head->rh, p);
	if (n != NULL) {
		debug(85, 1) ("bgp_rib_add_net: %s/%d: FOUND?!\n", inet_ntoa(addr), masklen);
		Deref_Prefix(p);
		return 0;
	}

	n = radix_lookup(head->rh, p);
	/* XXX should verify? */
	/* XXX should add some path data, etc? */
	Deref_Prefix(p);
	return 1;
}

int
bgp_rib_del_net(bgp_rib_head_t *head, struct in_addr addr, int masklen)
{
	prefix_t * p;
	radix_node_t *n;

	debug(85, 1) ("bgp_rib_del_net: %s/%d\n", inet_ntoa(addr), masklen);

	p = New_Prefix(AF_INET, &addr, masklen, NULL);

	n = radix_search_exact(head->rh, p);
	if (n == NULL) {
		debug(85, 1) ("bgp_rib_del_net: %s/%d: NOT FOUND?!\n", inet_ntoa(addr), masklen);
		Deref_Prefix(p);
		return 0;
	}
	/* XXX clear data associated with this prefix! */
	radix_remove(head->rh, n);
	Deref_Prefix(p);
	return 1;



}

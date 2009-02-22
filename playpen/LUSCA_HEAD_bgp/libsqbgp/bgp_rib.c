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

#include "bgp_packet.h"
#include "bgp_rib.h"
#include "bgp_core.h"

int
bgp_rib_match_net(bgp_rib_head_t *head, struct in_addr addr, int masklen)
{
    return 1;
}

/* initialize the radix tree structure */

extern int squid_max_keylen;	/* yuck.. this is in lib/radix.c */

void
bgp_rib_init(bgp_rib_head_t *head)
{
}

void
bgp_rib_destroy(bgp_rib_head_t *head)
{
}

void
bgp_rib_clean(bgp_rib_head_t *head)
{
}


int
bgp_rib_add_net(bgp_rib_head_t *head, struct in_addr addr, int masklen)
{
    return 1;
}

int
bgp_rib_del_net(bgp_rib_head_t *head, struct in_addr addr, int masklen)
{
    return 1;
}

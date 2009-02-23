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
#include "../include/hash.h"
#include "../libcore/tools.h"
#include "../libsqdebug/debug.h"

#include "bgp_aspath.h"


void
aspath_head_init(aspath_head_t *h)
{
	bzero(h, sizeof(*h));
}

void
aspath_head_free(aspath_head_t *h)
{

}

/*
 * Create an AS path. This does NOT check for duplicate
 * entries!
 */
aspath_entry_t *
aspath_create(u_int16_t *aspath, int aspath_cnt)
{

}

/*
 * create or ref an AS Path
 */
aspath_entry_t *
aspath_get(u_int16_t *aspath, int aspath_cnt)
{

}

/*
 * deref and potentially free the given as path
 */
void
aspath_deref(aspath_entry_t *a)
{

}

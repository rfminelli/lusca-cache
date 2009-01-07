
/*
 * Only compile this in if the other modules aren't included
 */

#if FREEBSD_TPROXY

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "../libstat/StatHist.h"
#include "../libsqinet/sqinet.h"
#include "fd_types.h"
#include "comm_types.h"
#include "globals.h"

int
comm_ips_bind(int fd, sqaddr_t *a)
{
    int on = 1;

    if (setsockopt(fd, IPPROTO_IP, IP_NONLOCALOK, (char *)&on, sizeof(on)) != 0)
        return COMM_ERROR;
    if (bind(fd, sqinet_get_entry(a), sqinet_get_length(a)) != 0)
        return COMM_ERROR;
    return COMM_OK;
}

void
comm_ips_keepCapabilities(void)
{
}

void
comm_ips_restoreCapabilities(int keep)
{
}

#endif

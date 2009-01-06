
/*
 * Only compile this in if the other modules aren't included
 */

#if (!LINUX_TPROXY)

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
    return COMM_ERROR;
}

void
comm_ips_keepCapabilities(void)
{
    need_linux_tproxy = 0;
}

void
comm_ips_restoreCapabilities(int keep)
{
    need_linux_tproxy = 0;
}

#endif

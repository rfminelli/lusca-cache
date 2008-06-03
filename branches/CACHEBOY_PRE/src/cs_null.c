
#include "squid.h"

int
cs_bind(int fd, struct in_addr addr, u_short port)
{
    return COMM_ERROR;
}

void
cs_keepCapabilities(void)
{
    need_linux_tproxy = 0;
}

void
cs_restoreCapabilities(int keep)
{
    need_linux_tproxy = 0;
}

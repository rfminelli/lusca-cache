
#include "squid.h"

#ifndef IP_TRANSPARENT
#error IP_TRANSPARENT must be defined for TPROXY4 support to functioN!
#endif

int
cs_bind(int fd, struct in_addr addr, u_short port)
{
    int tos = 1;
    struct sockaddr_in S;

    /* First enable the ability to override the bind restrictions */
    if (setsockopt(fd, SOL_IP, IP_TRANSPARENT, (char *) &tos, sizeof(int)) < 0) {
        debug(50, 1) ("comm_open: setsockopt(IP_TRANSPARENT) on FD %d failed: %s\n", fd, xstrerror());
        return COMM_ERROR;
    }

    /* Now do the non-local bind */
    memset(&S, '\0', sizeof(S));
    S.sin_family = AF_INET;
    S.sin_port = htons(port);
    S.sin_addr = addr;
    statCounter.syscalls.sock.binds++;
    if (bind(s, (struct sockaddr *) &S, sizeof(S)) == 0)
        return COMM_OK;
    debug(50, 1) ("comm_open: setsockopt(IP_TRANSPARENT) on FD %d failed: %s\n", fd, xstrerror());
    return COMM_ERROR;
}

void
cs_keepCapabilities(void)
{
}

void
cs_restoreCapabilities(int keep)
{
}

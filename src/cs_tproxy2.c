
#include "squid.h"
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv4/ip_tproxy.h>

#ifdef _SQUID_LINUX_
#if HAVE_SYS_CAPABILITY_H
#undef _POSIX_SOURCE
/* Ugly glue to get around linux header madness colliding with glibc */
#define _LINUX_TYPES_H
#define _LINUX_FS_H
typedef uint32_t __u32;
#include <sys/capability.h>
#endif
#endif

#if HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

int
cs_bind(int fd, struct in_addr addr, u_short port)
{
	struct in_tproxy itp;

        itp.v.addr.faddr.s_addr = fwdState->src.sin_addr.s_addr;
        itp.v.addr.fport = 0;
        
        /* If these syscalls fail then we just fallback to connecting
         * normally by simply ignoring the errors...
         */
        itp.op = TPROXY_ASSIGN;
        if (setsockopt(fd, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) == -1) {
            debug(20, 1) ("tproxy ip=%s,0x%x,port=%d ERROR ASSIGN\n",
               inet_ntoa(itp.v.addr.faddr),
               itp.v.addr.faddr.s_addr,
               itp.v.addr.fport);
	    return COMM_ERROR;
        }
        itp.op = TPROXY_FLAGS;
        itp.v.flags = ITP_CONNECT;
        if (setsockopt(fd, SOL_IP, IP_TPROXY, &itp, sizeof(itp)) == -1) {
           debug(20, 1) ("tproxy ip=%x,port=%d ERROR CONNECT\n",
               itp.v.addr.faddr.s_addr,
               itp.v.addr.fport);
           return COMM_ERROR;
        }
	return COMM_OK;
}

void
cs_keepCapabilities(void)
{
#if HAVE_PRCTL && defined(PR_SET_KEEPCAPS) && HAVE_SYS_CAPABILITY_H
    if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0)) {
	/* Silent failure unless TPROXY is required. Maybe not started as root */
	if (enable_linux_tproxy) {
		debug(1, 1) ("Error - Linux tproxy support requires capability setting which has failed.  Continuing without tproxy support\n");
		enable_linux_tproxy = 0;
	}
    }
#endif
}

static void
cs_restoreCapabilities(int keep)
{
#if defined(_SQUID_LINUX_) && HAVE_SYS_CAPABILITY_H
    cap_user_header_t head = (cap_user_header_t) xcalloc(1, sizeof(cap_user_header_t));
    cap_user_data_t cap = (cap_user_data_t) xcalloc(1, sizeof(cap_user_data_t));

    head->version = _LINUX_CAPABILITY_VERSION;
    if (capget(head, cap) != 0) {
	debug(50, 1) ("Can't get current capabilities\n");
	goto nocap;
    }
    if (head->version != _LINUX_CAPABILITY_VERSION) {
	debug(50, 1) ("Invalid capability version %d (expected %d)\n", head->version, _LINUX_CAPABILITY_VERSION);
	goto nocap;
    }
    head->pid = 0;

    cap->inheritable = 0;
    cap->effective = (1 << CAP_NET_BIND_SERVICE);
    if (enable_linux_tproxy)
	cap->effective |= (1 << CAP_NET_ADMIN) | (1 << CAP_NET_BROADCAST);
    if (!keep)
	cap->permitted &= cap->effective;
    if (capset(head, cap) != 0) {
	/* Silent failure unless TPROXY is required */
	if (enable_linux_tproxy)
	    debug(50, 1) ("Error enabling needed capabilities. Will continue without tproxy support\n");
	enable_linux_tproxy = 0;
    }
  nocap:
    xfree(head);
    xfree(cap);
#else
    if (enable_linux_tproxy)
	debug(50, 1) ("Missing needed capability support. Will continue without tproxy support\n");
    enable_linux_tproxy = 0;
#endif
}

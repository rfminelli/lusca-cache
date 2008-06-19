
/*
 * $Id$
 *
 * DEBUG: section 5     Socket Functions
 * AUTHOR: Harvest Derived
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

/* On native Windows, squid_mswin.h needs to know when we are compiling
 * comm.c for the correct handling of FD<=>socket magic
 */
#define COMM_C

#include "squid.h"

#if defined(_SQUID_CYGWIN_)
#include <sys/ioctl.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

typedef struct {
    char *host;
    u_short port;
    struct sockaddr_in S;
    CNCB *callback;
    void *data;
    struct in_addr in_addr;
    int fd;
    int tries;
    int addrcount;
    int connstart;
} ConnectStateData;

static PF commConnectFree;
static PF commConnectHandle;
static IPH commConnectDnsHandle;
static void commConnectCallback(ConnectStateData * cs, int status);
static int commResetFD(ConnectStateData * cs);
static int commRetryConnect(ConnectStateData * cs);
CBDATA_TYPE(ConnectStateData);

void
commConnectStart(int fd, const char *host, u_short port, CNCB * callback, void *data, struct in_addr *addr)
{
    ConnectStateData *cs;
    debug(5, 3) ("commConnectStart: FD %d, %s:%d\n", fd, host, (int) port);
    cs = cbdataAlloc(ConnectStateData);
    cs->fd = fd;
    cs->host = xstrdup(host);
    cs->port = port;
    cs->callback = callback;
    cs->data = data;
    if (addr != NULL) {
	cs->in_addr = *addr;
	cs->addrcount = 1;
    } else {
	cs->addrcount = 0;
    }
    cbdataLock(cs->data);
    comm_add_close_handler(fd, commConnectFree, cs);
    ipcache_nbgethostbyname(host, commConnectDnsHandle, cs);
}

static void
commConnectDnsHandle(const ipcache_addrs * ia, void *data)
{
    ConnectStateData *cs = data;
    if (ia == NULL) {
	/* If we've been given a default IP, use it */
	if (cs->addrcount > 0) {
	    fd_table[cs->fd].flags.dnsfailed = 1;
	    cs->connstart = squid_curtime;
	    commConnectHandle(cs->fd, cs);
	} else {
	    debug(5, 3) ("commConnectDnsHandle: Unknown host: %s\n", cs->host);
	    if (!dns_error_message) {
		dns_error_message = "Unknown DNS error";
		debug(5, 1) ("commConnectDnsHandle: Bad dns_error_message\n");
	    }
	    assert(dns_error_message != NULL);
	    commConnectCallback(cs, COMM_ERR_DNS);
	}
	return;
    }
    assert(ia->cur < ia->count);
    cs->in_addr = ia->in_addrs[ia->cur];
    if (Config.onoff.balance_on_multiple_ip)
	ipcacheCycleAddr(cs->host, NULL);
    cs->addrcount = ia->count;
    cs->connstart = squid_curtime;
    commConnectHandle(cs->fd, cs);
}

static void
commConnectCallback(ConnectStateData * cs, int status)
{
    CNCB *callback = cs->callback;
    void *data = cs->data;
    int fd = cs->fd;
    comm_remove_close_handler(fd, commConnectFree, cs);
    cs->callback = NULL;
    cs->data = NULL;
    commSetTimeout(fd, -1, NULL, NULL);
    commConnectFree(fd, cs);
    if (cbdataValid(data))
	callback(fd, status, data);
    cbdataUnlock(data);
}

static void
commConnectFree(int fd, void *data)
{
    ConnectStateData *cs = data;
    debug(5, 3) ("commConnectFree: FD %d\n", fd);
    if (cs->data)
	cbdataUnlock(cs->data);
    safe_free(cs->host);
    cbdataFree(cs);
}

/* Reset FD so that we can connect() again */
static int
commResetFD(ConnectStateData * cs)
{
    int fd2;
    fde *F;
    if (!cbdataValid(cs->data))
	return 0;
    CommStats.syscalls.sock.sockets++;
    fd2 = socket(AF_INET, SOCK_STREAM, 0);
    /* XXX why are there two increments? -adrian */
    CommStats.syscalls.sock.sockets++;
    if (fd2 < 0) {
	debug(5, 0) ("commResetFD: socket: %s\n", xstrerror());
	if (ENFILE == errno || EMFILE == errno)
	    fdAdjustReserved();
	return 0;
    }
    /* We are about to close the fd (dup2 over it). Unregister from the event loop */
    commSetEvents(cs->fd, 0, 0);
#ifdef _SQUID_MSWIN_
    /* On Windows dup2() can't work correctly on Sockets, the          */
    /* workaround is to close the destination Socket before call them. */
    close(cs->fd);
#endif
    if (dup2(fd2, cs->fd) < 0) {
	debug(5, 0) ("commResetFD: dup2: %s\n", xstrerror());
	if (ENFILE == errno || EMFILE == errno)
	    fdAdjustReserved();
	close(fd2);
	return 0;
    }
    close(fd2);
    F = &fd_table[cs->fd];
    fd_table[cs->fd].flags.called_connect = 0;
    /*
     * yuck, this has assumptions about comm_open() arguments for
     * the original socket
     */
    if (commBind(cs->fd, F->local_addr, F->local_port) != COMM_OK) {
	debug(5, 0) ("commResetFD: bind: %s\n", xstrerror());
	return 0;
    }
#ifdef IP_TOS
    if (F->tos) {
	int tos = F->tos;
	if (setsockopt(cs->fd, IPPROTO_IP, IP_TOS, (char *) &tos, sizeof(int)) < 0)
	        debug(5, 1) ("commResetFD: setsockopt(IP_TOS) on FD %d: %s\n", cs->fd, xstrerror());
    }
#endif
    if (F->flags.close_on_exec)
	commSetCloseOnExec(cs->fd);
    if (F->flags.nonblocking)
	commSetNonBlocking(cs->fd);
#ifdef TCP_NODELAY
    if (F->flags.nodelay)
	commSetTcpNoDelay(cs->fd);
#endif

    /* Register the new FD with the event loop */
    commUpdateEvents(cs->fd);
    if (Config.tcpRcvBufsz > 0)
	commSetTcpRcvbuf(cs->fd, Config.tcpRcvBufsz);
    return 1;
}

static int
commRetryConnect(ConnectStateData * cs)
{
    assert(cs->addrcount > 0);
    if (cs->addrcount == 1) {
	if (cs->tries >= Config.retry.maxtries)
	    return 0;
	if (squid_curtime - cs->connstart > Config.Timeout.connect)
	    return 0;
    } else {
	if (cs->tries > cs->addrcount)
	    return 0;
    }
    return commResetFD(cs);
}

static void
commReconnect(void *data)
{
    ConnectStateData *cs = data;
    ipcache_nbgethostbyname(cs->host, commConnectDnsHandle, cs);
}

/* Connect SOCK to specified DEST_PORT at DEST_HOST. */
static void
commConnectHandle(int fd, void *data)
{
    ConnectStateData *cs = data;
    if (cs->S.sin_addr.s_addr == 0) {
	cs->S.sin_family = AF_INET;
	cs->S.sin_addr = cs->in_addr;
	cs->S.sin_port = htons(cs->port);
    }
    switch (comm_connect_addr(fd, &cs->S)) {
    case COMM_INPROGRESS:
	debug(5, 5) ("commConnectHandle: FD %d: COMM_INPROGRESS\n", fd);
	commSetSelect(fd, COMM_SELECT_WRITE, commConnectHandle, cs, 0);
	break;
    case COMM_OK:
	ipcacheMarkGoodAddr(cs->host, cs->S.sin_addr);
	commConnectCallback(cs, COMM_OK);
	break;
    default:
	cs->tries++;
	ipcacheMarkBadAddr(cs->host, cs->S.sin_addr);
	if (Config.onoff.test_reachability)
	    netdbDeleteAddrNetwork(cs->S.sin_addr);
	if (commRetryConnect(cs)) {
	    eventAdd("commReconnect", commReconnect, cs, cs->addrcount == 1 ? 0.05 : 0.0, 0);
	} else {
	    commConnectCallback(cs, COMM_ERR_CONNECT);
	}
	break;
    }
}


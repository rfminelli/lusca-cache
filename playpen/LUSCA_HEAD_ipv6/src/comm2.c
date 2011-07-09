/*
 vim:expandtab
 */

/*
 * $Id: comm.c 14853 2011-07-02 06:10:25Z adrian.chadd $
 *
 * DEBUG: section 5     Socket Functions - Replacement
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

#include "comm2.h"

static PF commConnectFree;
static PF commConnectHandle;
static IPH commConnectDnsHandle;
static void commConnectCallbackNew(ConnectStateDataNew * cs, int status);
static int commRetryConnect(ConnectStateDataNew * cs);
CBDATA_TYPE(ConnectStateDataNew);

/*
 * Setup a comm connect (new) structure for a future "start" call.
 */
ConnectStateDataNew *
commConnectStartNewSetup(const char *host, u_short port, CNCB *callback,
    void *data, sqaddr_t *addr6, int flags, const char *note)
{
    ConnectStateDataNew *cs;
    debug(5, 3) ("%s: new connection to %s:%d\n", __func__, host, (int) port);
    CBDATA_INIT_TYPE(ConnectStateDataNew);
    cs = cbdataAlloc(ConnectStateDataNew);
    cs->fd = -1;                /* Will need to be created */
    cs->host = xstrdup(host);
    cs->port = port;
    cs->callback = callback;
    cs->data = data;
    cs->comm_flags = flags;
    cs->comm_tos = 0;
    cs->comm_flags = flags;
    cs->start_time = cs->timeout = 0;

    sqinet_init(&cs->in_addr6);
    sqinet_init(&cs->lcl_addr4);
    sqinet_init(&cs->lcl_addr6);

    sqinet_set_family(&cs->lcl_addr4, AF_INET);
    sqinet_set_family(&cs->lcl_addr6, AF_INET6);
    sqinet_set_anyaddr(&cs->lcl_addr4);
    sqinet_set_anyaddr(&cs->lcl_addr6);

    /* Do we have a local address? Use it */
    if (addr6 != NULL) {
        sqinet_init(&cs->in_addr6);
        sqinet_copy(&cs->in_addr6, addr6);
        cs->addrcount = 1;
    } else {
        cs->addrcount = 0;
    }
    cbdataLock(cs->data);

    return cs;
}

void
commConnectNewSetupOutgoingV4(ConnectStateDataNew *cs, struct in_addr lcl)
{
        sqinet_set_v4_inaddr(&cs->lcl_addr4, &lcl);
}

void
commConnectNewSetupOutgoingV6(ConnectStateDataNew *cs, sqaddr_t *lcl)
{
        sqinet_copy(&cs->lcl_addr6, lcl);
}

void
commConnectNewSetTimeout(ConnectStateDataNew *cs, int timeout)
{
        cs->timeout = timeout;
}

void
commConnectNewSetTOS(ConnectStateDataNew *cs, int tos)
{
        cs->comm_tos = tos;
}

/*
 * Attempt to connect to host:port.
 * addr6 can specify a fixed v4 or v6 address; or NULL for host lookup.
 *
 * flags: additional comm flags
 * tos: TOS for newly created sockets
 * note: note for newly created sockets
 */
void
commConnectStartNewBegin(ConnectStateDataNew *cs)
{
    cs->start_time = squid_curtime;
    /* Begin the host lookup */
    ipcache_nbgethostbyname(cs->host, commConnectDnsHandle, cs);
}


/*
 * Close the destination socket.
 */
static void
commConnectCloseSocket(ConnectStateDataNew *cs)
{
        if (cs->fd == -1)
                return;
        debug(5, 1) ("%s: FD (%d): closing\n", __func__, cs->fd);
        comm_remove_close_handler(cs->fd, commConnectFree, cs);
        comm_close(cs->fd);
        cs->fd = -1;
}

/*
 * Create a comm socket matching the destination
 * record.
 */
static int
commConnectCreateSocket(ConnectStateDataNew *cs)
{
        int af;

        /* Does a socket exist? It shouldn't at this point. */
        if (cs->fd != -1) {
                debug(5, 1) ("%s: FD (%d) exists when it shouldn't!\n",
                    __func__, cs->fd);
                comm_remove_close_handler(cs->fd, commConnectFree, cs);
                comm_close(cs->fd);
                cs->fd = -1;
        }

        /* Create a new socket for the given destination address */
        af = sqinet_get_family(&cs->in_addr6);

        /* Open with the correct local address */
        cs->fd = comm_open6(SOCK_STREAM, IPPROTO_TCP, (af == AF_INET ?
            &cs->lcl_addr4 : &cs->lcl_addr6),
            cs->comm_flags | COMM_NONBLOCKING, cs->comm_tos, cs->comm_note);

        /* Did socket creation fail? Then pass it up the stack */
        if (cs->fd == -1)
                return -1;

        /* Setup the close handler */
        comm_add_close_handler(cs->fd, commConnectFree, cs);

        return cs->fd;
}

static void
commConnectDnsHandle(const ipcache_addrs * ia, void *data)
{
    ConnectStateDataNew *cs = data;
    if (ia == NULL) {
        /* If we've been given a default IP, use it */
        if (cs->addrcount > 0) {
            fd_table[cs->fd].flags.dnsfailed = 1;
            cs->connstart = squid_curtime;
            if (commConnectCreateSocket(cs) == -1) {
                debug(5, 3) ("%s: socket problem: %s\n", __func__, cs->host);
                commConnectCallbackNew(cs, COMM_ERR_CONNECT);
                return;
            }
            commConnectHandle(cs->fd, cs);
        } else {
            debug(5, 3) ("commConnectDnsHandle: Unknown host: %s\n", cs->host);
            if (!dns_error_message) {
                dns_error_message = "Unknown DNS error";
                debug(5, 1) ("commConnectDnsHandle: Bad dns_error_message\n");
            }
            assert(dns_error_message != NULL);
            commConnectCallbackNew(cs, COMM_ERR_DNS);
        }
        return;
    }
    assert(ia->cur < ia->count);
    sqinet_done(&cs->in_addr6);
    sqinet_init(&cs->in_addr6);
    (void) ipcacheGetAddr(ia, ia->cur, &cs->in_addr6);
    if (Config.onoff.balance_on_multiple_ip)
        ipcacheCycleAddr(cs->host, NULL);
    cs->addrcount = ia->count;
    cs->connstart = squid_curtime;

    /* Create the initial outbound socket */
    if (commConnectCreateSocket(cs) == -1) {
        debug(5, 3) ("%s: socket problem: %s\n", __func__, cs->host);
        commConnectCallbackNew(cs, COMM_ERR_CONNECT);
        return;
    }
    commConnectHandle(cs->fd, cs);
}

static void
commConnectCallbackNew(ConnectStateDataNew * cs, int status)
{
    CNCB *callback = cs->callback;
    void *data = cs->data;
    int fd = cs->fd;
    if (fd != -1) {
        comm_remove_close_handler(fd, commConnectFree, cs);
        commSetTimeout(fd, -1, NULL, NULL);
    }
    cs->callback = NULL;
    cs->data = NULL;
    if (status != COMM_OK) {
        if (fd != -1)
            comm_close(fd);
        fd = cs->fd = -1;
    }
    commConnectFree(fd, cs);
    if (cbdataValid(data))
        callback(fd, status, data);
    cbdataUnlock(data);
}

static void
commConnectFree(int fd, void *data)
{
    ConnectStateDataNew *cs = data;
    debug(5, 3) ("commConnectFree: FD %d\n", fd);
    if (cs->data)
        cbdataUnlock(cs->data);
    safe_free(cs->host);
    sqinet_done(&cs->in_addr6);
    sqinet_done(&cs->lcl_addr4);
    sqinet_done(&cs->lcl_addr6);
    cbdataFree(cs);
}

static int
commRetryConnect(ConnectStateDataNew * cs)
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

    /* The next retry may be a different protocol family */
    commConnectCloseSocket(cs);
    if (commConnectCreateSocket(cs) != -1)
        return 0;
    return 1;
}

static void
commReconnect(void *data)
{
    ConnectStateDataNew *cs = data;
    ipcache_nbgethostbyname(cs->host, commConnectDnsHandle, cs);
}

/* Connect SOCK to specified DEST_PORT at DEST_HOST. */
static void
commConnectHandle(int fd, void *data)
{
    int r;
    sqaddr_t a;

    ConnectStateDataNew *cs = data;

    if (cs->fd == -1) {
        debug(5, 1) ("%s: shouldn't have FD=-1, barfing\n", __func__);
        commConnectCallbackNew(cs, COMM_ERR_CONNECT);
        return;
    }

    /* Create a temporary sqaddr_t which also contains the port we're connecting to */
    /* This should eventually just be folded into cs->in_addr6 -adrian */
    sqinet_init(&a);
    sqinet_copy(&a, &cs->in_addr6);
    sqinet_set_port(&a, cs->port, SQADDR_NONE);
    r = comm_connect_addr(fd, &a);
    sqinet_done(&a);
    switch(r) {
    case COMM_INPROGRESS:
        debug(5, 5) ("commConnectHandle: FD %d: COMM_INPROGRESS\n", fd);
        commSetSelect(fd, COMM_SELECT_WRITE, commConnectHandle, cs, 0);
        break;
    case COMM_OK:
        ipcacheMarkGoodAddr(cs->host, &cs->in_addr6);
        commConnectCallbackNew(cs, COMM_OK);
        break;
    default:
        cs->tries++;
        ipcacheMarkBadAddr(cs->host, &cs->in_addr6);
        if (Config.onoff.test_reachability)
            netdbDeleteAddrNetwork(&cs->in_addr6);
        if (commRetryConnect(cs)) {
            eventAdd("commReconnect", commReconnect, cs, cs->addrcount == 1 ? 0.05 : 0.0, 0);
        } else {
            commConnectCallbackNew(cs, COMM_ERR_CONNECT);
        }
        break;
    }
}


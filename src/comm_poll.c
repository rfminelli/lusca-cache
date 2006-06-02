
/*
 * $Id$
 *
 * DEBUG: section 5     Socket Functions
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

#include "squid.h"

#include <sys/poll.h>

static int MAX_POLL_TIME = 1000;	/* see also comm_quick_poll_required() */

#ifndef        howmany
#define howmany(x, y)   (((x)+((y)-1))/(y))
#endif
#ifndef        NBBY
#define        NBBY    8
#endif
#define FD_MASK_BYTES sizeof(fd_mask)
#define FD_MASK_BITS (FD_MASK_BYTES*NBBY)

/* STATIC */
static int fdIsHttp(int fd);
static int fdIsIcp(int fd);
static int fdIsDns(int fd);
static int commDeferRead(int fd);
static void checkTimeouts(void);
static OBJH commIncomingStats;
static int comm_check_incoming_poll_handlers(int nfds, int *fds);
static void comm_poll_dns_incoming(void);

/*
 * Automatic tuning for incoming requests:
 *
 * INCOMING sockets are the ICP and HTTP ports.  We need to check these
 * fairly regularly, but how often?  When the load increases, we
 * want to check the incoming sockets more often.  If we have a lot
 * of incoming ICP, then we need to check these sockets more than
 * if we just have HTTP.
 *
 * The variables 'incoming_icp_interval' and 'incoming_http_interval' 
 * determine how many normal I/O events to process before checking
 * incoming sockets again.  Note we store the incoming_interval
 * multipled by a factor of (2^INCOMING_FACTOR) to have some
 * pseudo-floating point precision.
 *
 * The variable 'icp_io_events' and 'http_io_events' counts how many normal
 * I/O events have been processed since the last check on the incoming
 * sockets.  When io_events > incoming_interval, its time to check incoming
 * sockets.
 *
 * Every time we check incoming sockets, we count how many new messages
 * or connections were processed.  This is used to adjust the
 * incoming_interval for the next iteration.  The new incoming_interval
 * is calculated as the current incoming_interval plus what we would
 * like to see as an average number of events minus the number of
 * events just processed.
 *
 *  incoming_interval = incoming_interval + target_average - number_of_events_processed
 *
 * There are separate incoming_interval counters for both HTTP and ICP events
 * 
 * You can see the current values of the incoming_interval's, as well as
 * a histogram of 'incoming_events' by asking the cache manager
 * for 'comm_incoming', e.g.:
 *
 *      % ./client mgr:comm_incoming
 *
 * Caveats:
 *
 *      - We have MAX_INCOMING_INTEGER as a magic upper limit on
 *        incoming_interval for both types of sockets.  At the
 *        largest value the cache will effectively be idling.
 *
 *      - The higher the INCOMING_FACTOR, the slower the algorithm will
 *        respond to load spikes/increases/decreases in demand. A value
 *        between 3 and 8 is recommended.
 */

#define MAX_INCOMING_INTEGER 256
#define INCOMING_FACTOR 5
#define MAX_INCOMING_INTERVAL (MAX_INCOMING_INTEGER << INCOMING_FACTOR)
static int icp_io_events = 0;
static int dns_io_events = 0;
static int http_io_events = 0;
static int incoming_icp_interval = 16 << INCOMING_FACTOR;
static int incoming_dns_interval = 16 << INCOMING_FACTOR;
static int incoming_http_interval = 16 << INCOMING_FACTOR;
#define commCheckICPIncoming (++icp_io_events > (incoming_icp_interval>> INCOMING_FACTOR))
#define commCheckDNSIncoming (++dns_io_events > (incoming_dns_interval>> INCOMING_FACTOR))
#define commCheckHTTPIncoming (++http_io_events > (incoming_http_interval>> INCOMING_FACTOR))

static int
fdIsIcp(int fd)
{
    if (fd == theInIcpConnection)
	return 1;
    if (fd == theOutIcpConnection)
	return 1;
    return 0;
}

static int
fdIsDns(int fd)
{
    if (fd == DnsSocket)
	return 1;
    return 0;
}

static int
fdIsHttp(int fd)
{
    int j;
    for (j = 0; j < NHttpSockets; j++) {
	if (fd == HttpSockets[j])
	    return 1;
    }
    return 0;
}

#if DELAY_POOLS
static int slowfdcnt = 0;
static int slowfdarr[SQUID_MAXFD];

static void
commAddSlowFd(int fd)
{
    assert(slowfdcnt < SQUID_MAXFD);
    slowfdarr[slowfdcnt++] = fd;
}

static int
commGetSlowFd(void)
{
    int whichfd, retfd;

    if (!slowfdcnt)
	return -1;
    whichfd = squid_random() % slowfdcnt;
    retfd = slowfdarr[whichfd];
    slowfdarr[whichfd] = slowfdarr[--slowfdcnt];
    return retfd;
}
#endif

static int
comm_check_incoming_poll_handlers(int nfds, int *fds)
{
    int i;
    int fd;
    PF *hdl = NULL;
    int npfds;
    struct pollfd pfds[3 + MAXHTTPPORTS];
    incoming_sockets_accepted = 0;
    for (i = npfds = 0; i < nfds; i++) {
	int events;
	fd = fds[i];
	events = 0;
	if (fd_table[fd].read_handler)
	    events |= POLLRDNORM;
	if (fd_table[fd].write_handler)
	    events |= POLLWRNORM;
	if (events) {
	    pfds[npfds].fd = fd;
	    pfds[npfds].events = events;
	    pfds[npfds].revents = 0;
	    npfds++;
	}
    }
    if (!nfds)
	return -1;
    getCurrentTime();
    statCounter.syscalls.polls++;
    if (poll(pfds, npfds, 0) < 1)
	return incoming_sockets_accepted;
    for (i = 0; i < npfds; i++) {
	int revents;
	if (((revents = pfds[i].revents) == 0) || ((fd = pfds[i].fd) == -1))
	    continue;
	if (revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR)) {
	    if ((hdl = fd_table[fd].read_handler)) {
		fd_table[fd].read_handler = NULL;
		hdl(fd, fd_table[fd].read_data);
	    } else if (pfds[i].events & POLLRDNORM)
		debug(5, 1) ("comm_poll_incoming: FD %d NULL read handler\n",
		    fd);
	}
	if (revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR)) {
	    if ((hdl = fd_table[fd].write_handler)) {
		fd_table[fd].write_handler = NULL;
		hdl(fd, fd_table[fd].write_data);
	    } else if (pfds[i].events & POLLWRNORM)
		debug(5, 1) ("comm_poll_incoming: FD %d NULL write_handler\n",
		    fd);
	}
    }
    return incoming_sockets_accepted;
}

static void
comm_poll_icp_incoming(void)
{
    int nfds = 0;
    int fds[2];
    int nevents;
    icp_io_events = 0;
    if (theInIcpConnection >= 0)
	fds[nfds++] = theInIcpConnection;
    if (theInIcpConnection != theOutIcpConnection)
	if (theOutIcpConnection >= 0)
	    fds[nfds++] = theOutIcpConnection;
    if (nfds == 0)
	return;
    nevents = comm_check_incoming_poll_handlers(nfds, fds);
    incoming_icp_interval += Config.comm_incoming.icp_average - nevents;
    if (incoming_icp_interval < Config.comm_incoming.icp_min_poll)
	incoming_icp_interval = Config.comm_incoming.icp_min_poll;
    if (incoming_icp_interval > MAX_INCOMING_INTERVAL)
	incoming_icp_interval = MAX_INCOMING_INTERVAL;
    if (nevents > INCOMING_ICP_MAX)
	nevents = INCOMING_ICP_MAX;
    statHistCount(&statCounter.comm_icp_incoming, nevents);
}

static void
comm_poll_http_incoming(void)
{
    int nfds = 0;
    int fds[MAXHTTPPORTS];
    int j;
    int nevents;
    http_io_events = 0;
    for (j = 0; j < NHttpSockets; j++) {
	if (HttpSockets[j] < 0)
	    continue;
	if (commDeferRead(HttpSockets[j]))
	    continue;
	fds[nfds++] = HttpSockets[j];
    }
    nevents = comm_check_incoming_poll_handlers(nfds, fds);
    incoming_http_interval = incoming_http_interval
	+ Config.comm_incoming.http_average - nevents;
    if (incoming_http_interval < Config.comm_incoming.http_min_poll)
	incoming_http_interval = Config.comm_incoming.http_min_poll;
    if (incoming_http_interval > MAX_INCOMING_INTERVAL)
	incoming_http_interval = MAX_INCOMING_INTERVAL;
    if (nevents > INCOMING_HTTP_MAX)
	nevents = INCOMING_HTTP_MAX;
    statHistCount(&statCounter.comm_http_incoming, nevents);
}

/* poll all sockets; call handlers for those that are ready. */
int
comm_poll(int msec)
{
    struct pollfd pfds[SQUID_MAXFD];
#if DELAY_POOLS
    fd_set slowfds;
#endif
    int fd;
    unsigned int i;
    unsigned int maxfd;
    unsigned int nfds;
    unsigned int npending;
    int num;
    int callicp = 0, callhttp = 0;
    int calldns = 0;
    static time_t last_timeout = 0;
    double timeout = current_dtime + (msec / 1000.0);
    do {
	double start;
	getCurrentTime();
	start = current_dtime;
	/* Handle any fs callbacks that need doing */
	storeDirCallback();
#if DELAY_POOLS
	FD_ZERO(&slowfds);
#endif
	if (commCheckICPIncoming)
	    comm_poll_icp_incoming();
	if (commCheckDNSIncoming)
	    comm_poll_dns_incoming();
	if (commCheckHTTPIncoming)
	    comm_poll_http_incoming();
	callicp = calldns = callhttp = 0;
	nfds = 0;
	npending = 0;
	maxfd = Biggest_FD + 1;
	for (i = 0; i < maxfd; i++) {
	    int events;
	    events = 0;
	    /* Check each open socket for a handler. */
	    if (fd_table[i].read_handler) {
		int dopoll = 1;
		switch (commDeferRead(i)) {
		case 0:
		    break;
		case 1:
		    dopoll = 0;
		    break;
#if DELAY_POOLS
		case -1:
		    FD_SET(i, &slowfds);
		    break;
#endif
		default:
		    fatalf("bad return value from commDeferRead(FD %d)\n", i);
		    /* NOTREACHED */
		}
		if (dopoll) {
		    switch (fd_table[i].read_pending) {
		    case COMM_PENDING_NORMAL:
			events |= POLLRDNORM;
			break;
		    case COMM_PENDING_WANTS_WRITE:
			events |= POLLWRNORM;
			break;
		    case COMM_PENDING_WANTS_READ:
			events |= POLLRDNORM;
			break;
		    case COMM_PENDING_NOW:
			events |= POLLRDNORM;
			npending++;
			break;
		    }
		}
	    }
	    if (fd_table[i].write_handler) {
		switch (fd_table[i].write_pending) {
		case COMM_PENDING_NORMAL:
		    events |= POLLWRNORM;
		    break;
		case COMM_PENDING_WANTS_WRITE:
		    events |= POLLWRNORM;
		    break;
		case COMM_PENDING_WANTS_READ:
		    events |= POLLRDNORM;
		    break;
		case COMM_PENDING_NOW:
		    events |= POLLWRNORM;
		    npending++;
		    break;
		}
	    }
	    if (events) {
		pfds[nfds].fd = i;
		pfds[nfds].events = events;
		pfds[nfds].revents = 0;
		nfds++;
	    }
	}
	if (nfds == 0) {
	    assert(shutting_down);
	    return COMM_SHUTDOWN;
	}
	if (npending)
	    msec = 0;
	if (msec > MAX_POLL_TIME)
	    msec = MAX_POLL_TIME;
	statCounter.syscalls.polls++;
	num = poll(pfds, nfds, msec);
	statCounter.select_loops++;
	if (num < 0 && !ignoreErrno(errno)) {
	    debug(5, 0) ("comm_poll: poll failure: %s\n", xstrerror());
	    assert(errno != EINVAL);
	    return COMM_ERROR;
	    /* NOTREACHED */
	}
	debug(5, num ? 5 : 8) ("comm_poll: %d+%u FDs ready\n", num, npending);
	statHistCount(&statCounter.select_fds_hist, num);
	/* Check timeout handlers ONCE each second. */
	if (squid_curtime > last_timeout) {
	    last_timeout = squid_curtime;
	    checkTimeouts();
	}
	if (num <= 0 && npending == 0)
	    continue;
	/* scan each socket but the accept socket. Poll this 
	 * more frequently to minimize losses due to the 5 connect 
	 * limit in SunOS */
	for (i = 0; i < nfds; i++) {
	    fde *F;
	    int revents = pfds[i].revents;
	    fd = pfds[i].fd;
	    if (fd == -1)
		continue;
	    switch (fd_table[fd].read_pending) {
	    case COMM_PENDING_NORMAL:
	    case COMM_PENDING_WANTS_READ:
		break;
	    case COMM_PENDING_WANTS_WRITE:
		if (pfds[i].revents & (POLLOUT | POLLWRNORM))
		    revents |= POLLIN;
		break;
	    case COMM_PENDING_NOW:
		revents |= POLLIN;
		break;
	    }
	    switch (fd_table[fd].write_pending) {
	    case COMM_PENDING_NORMAL:
	    case COMM_PENDING_WANTS_WRITE:
		break;
	    case COMM_PENDING_WANTS_READ:
		if (pfds[i].revents & (POLLIN | POLLRDNORM))
		    revents |= POLLOUT;
		break;
	    case COMM_PENDING_NOW:
		revents |= POLLOUT;
		break;
	    }
	    if (revents == 0)
		continue;
	    if (fdIsIcp(fd)) {
		callicp = 1;
		continue;
	    }
	    if (fdIsDns(fd)) {
		calldns = 1;
		continue;
	    }
	    if (fdIsHttp(fd)) {
		callhttp = 1;
		continue;
	    }
	    F = &fd_table[fd];
	    if (revents & (POLLRDNORM | POLLIN | POLLHUP | POLLERR)) {
		PF *hdl = F->read_handler;
		debug(5, 6) ("comm_poll: FD %d ready for reading\n", fd);
		if (hdl == NULL)
		    (void) 0;	/* Nothing to do */
#if DELAY_POOLS
		else if (FD_ISSET(fd, &slowfds))
		    commAddSlowFd(fd);
#endif
		else {
		    F->read_handler = NULL;
		    F->read_pending = COMM_PENDING_NORMAL;
		    hdl(fd, F->read_data);
		    statCounter.select_fds++;
		    if (commCheckICPIncoming)
			comm_poll_icp_incoming();
		    if (commCheckDNSIncoming)
			comm_poll_dns_incoming();
		    if (commCheckHTTPIncoming)
			comm_poll_http_incoming();
		}
	    }
	    if (revents & (POLLWRNORM | POLLOUT | POLLHUP | POLLERR)) {
		PF *hdl = F->write_handler;
		debug(5, 5) ("comm_poll: FD %d ready for writing\n", fd);
		if (hdl != NULL) {
		    F->write_handler = NULL;
		    F->write_pending = COMM_PENDING_NORMAL;
		    hdl(fd, F->write_data);
		    statCounter.select_fds++;
		    if (commCheckICPIncoming)
			comm_poll_icp_incoming();
		    if (commCheckDNSIncoming)
			comm_poll_dns_incoming();
		    if (commCheckHTTPIncoming)
			comm_poll_http_incoming();
		}
	    }
	    if (revents & POLLNVAL) {
		close_handler *ch;
		debug(5, 0) ("WARNING: FD %d has handlers, but it's invalid.\n", fd);
		debug(5, 0) ("FD %d is a %s\n", fd, fdTypeStr[F->type]);
		debug(5, 0) ("--> %s\n", F->desc);
		debug(5, 0) ("tmout:%p read:%p write:%p\n",
		    F->timeout_handler,
		    F->read_handler,
		    F->write_handler);
		for (ch = F->close_handler; ch; ch = ch->next)
		    debug(5, 0) (" close handler: %p\n", ch->handler);
		if (F->close_handler) {
		    commCallCloseHandlers(fd);
		} else if (F->timeout_handler) {
		    debug(5, 0) ("comm_poll: Calling Timeout Handler\n");
		    F->timeout_handler(fd, F->timeout_data);
		}
		F->close_handler = NULL;
		F->timeout_handler = NULL;
		F->read_handler = NULL;
		F->write_handler = NULL;
		if (F->flags.open)
		    fd_close(fd);
	    }
	}
	if (callicp)
	    comm_poll_icp_incoming();
	if (calldns)
	    comm_poll_dns_incoming();
	if (callhttp)
	    comm_poll_http_incoming();
#if DELAY_POOLS
	while ((fd = commGetSlowFd()) != -1) {
	    fde *F = &fd_table[fd];
	    PF *hdl = F->read_handler;
	    debug(5, 6) ("comm_select: slow FD %d selected for reading\n", fd);
	    if (hdl != NULL) {
		F->read_handler = NULL;
		F->read_pending = COMM_PENDING_NORMAL;
		hdl(fd, F->read_data);
		statCounter.select_fds++;
		if (commCheckICPIncoming)
		    comm_poll_icp_incoming();
		if (commCheckDNSIncoming)
		    comm_poll_dns_incoming();
		if (commCheckHTTPIncoming)
		    comm_poll_http_incoming();
	    }
	}
#endif
	getCurrentTime();
	statCounter.select_time += (current_dtime - start);
	return COMM_OK;
    }
    while (timeout > current_dtime);
    debug(5, 8) ("comm_poll: time out: %ld.\n", (long int) squid_curtime);
    return COMM_TIMEOUT;
}

static void
comm_poll_dns_incoming(void)
{
    int nfds = 0;
    int fds[2];
    int nevents;
    dns_io_events = 0;
    if (DnsSocket < 0)
	return;
    fds[nfds++] = DnsSocket;
    nevents = comm_check_incoming_poll_handlers(nfds, fds);
    if (nevents < 0)
	return;
    incoming_dns_interval += Config.comm_incoming.dns_average - nevents;
    if (incoming_dns_interval < Config.comm_incoming.dns_min_poll)
	incoming_dns_interval = Config.comm_incoming.dns_min_poll;
    if (incoming_dns_interval > MAX_INCOMING_INTERVAL)
	incoming_dns_interval = MAX_INCOMING_INTERVAL;
    if (nevents > INCOMING_DNS_MAX)
	nevents = INCOMING_DNS_MAX;
    statHistCount(&statCounter.comm_dns_incoming, nevents);
}

void
comm_select_init(void)
{
    cachemgrRegister("comm_incoming",
	"comm_incoming() stats",
	commIncomingStats, 0, 1);
}

void
comm_select_shutdown(void)
{
}

static void
commIncomingStats(StoreEntry * sentry)
{
    StatCounters *f = &statCounter;
    storeAppendPrintf(sentry, "Current incoming_icp_interval: %d\n",
	incoming_icp_interval >> INCOMING_FACTOR);
    storeAppendPrintf(sentry, "Current incoming_dns_interval: %d\n",
	incoming_dns_interval >> INCOMING_FACTOR);
    storeAppendPrintf(sentry, "Current incoming_http_interval: %d\n",
	incoming_http_interval >> INCOMING_FACTOR);
    storeAppendPrintf(sentry, "\n");
    storeAppendPrintf(sentry, "Histogram of events per incoming socket type\n");
    storeAppendPrintf(sentry, "ICP Messages handled per comm_poll_icp_incoming() call:\n");
    statHistDump(&f->comm_icp_incoming, sentry, statHistIntDumper);
    storeAppendPrintf(sentry, "DNS Messages handled per comm_poll_dns_incoming() call:\n");
    statHistDump(&f->comm_dns_incoming, sentry, statHistIntDumper);
    storeAppendPrintf(sentry, "HTTP Messages handled per comm_poll_http_incoming() call:\n");
    statHistDump(&f->comm_http_incoming, sentry, statHistIntDumper);
}

void
commSetEvents(int fd, int need_read, int need_write, int force)
{
    /* XXX Here we could optimize the poll arrays quite considerably */
}

static int
commDeferRead(int fd)
{
    fde *F = &fd_table[fd];
    if (F->defer_check == NULL)
	return 0;
    return F->defer_check(fd, F->defer_data);
}

static void
checkTimeouts(void)
{
    int fd;
    fde *F = NULL;
    PF *callback;
    for (fd = 0; fd <= Biggest_FD; fd++) {
	F = &fd_table[fd];
	if (!F->flags.open)
	    continue;
	if (F->timeout == 0)
	    continue;
	if (F->timeout > squid_curtime)
	    continue;
	debug(5, 5) ("checkTimeouts: FD %d Expired\n", fd);
	if (F->timeout_handler) {
	    debug(5, 5) ("checkTimeouts: FD %d: Call timeout handler\n", fd);
	    callback = F->timeout_handler;
	    F->timeout_handler = NULL;
	    callback(fd, F->timeout_data);
	} else {
	    debug(5, 5) ("checkTimeouts: FD %d: Forcing comm_close()\n", fd);
	    comm_close(fd);
	}
    }
}


/* Called by async-io or diskd to speed up the polling */
void
comm_quick_poll_required(void)
{
    MAX_POLL_TIME = 10;
}

/* Defer reads from this fd */
void
commDeferFD(int fd)
{
    /* Not implemented */
}

/* Resume reading from the given fd */
void
commResumeFD(int fd)
{
    /* Not implemented */
}

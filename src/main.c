/*
 * $Id$
 *
 * DEBUG: section 1	Startup and Main Loop
 * AUTHOR: Harvest Derived
 *
 * SQUID Internet Object Cache  http://www.nlanr.net/Squid/
 * --------------------------------------------------------
 *
 *   Squid is the result of efforts by numerous individuals from the
 *   Internet community.  Development is led by Duane Wessels of the
 *   National Laboratory for Applied Network Research and funded by
 *   the National Science Foundation.
 * 
 */
 
/*
 * Copyright (c) 1994, 1995.  All rights reserved.
 *  
 *   The Harvest software was developed by the Internet Research Task
 *   Force Research Group on Resource Discovery (IRTF-RD):
 *  
 *         Mic Bowman of Transarc Corporation.
 *         Peter Danzig of the University of Southern California.
 *         Darren R. Hardy of the University of Colorado at Boulder.
 *         Udi Manber of the University of Arizona.
 *         Michael F. Schwartz of the University of Colorado at Boulder.
 *         Duane Wessels of the University of Colorado at Boulder.
 *  
 *   This copyright notice applies to software in the Harvest
 *   ``src/'' directory only.  Users should consult the individual
 *   copyright notices in the ``components/'' subdirectories for
 *   copyright information about other software bundled with the
 *   Harvest source code distribution.
 *  
 * TERMS OF USE
 *   
 *   The Harvest software may be used and re-distributed without
 *   charge, provided that the software origin and research team are
 *   cited in any use of the system.  Most commonly this is
 *   accomplished by including a link to the Harvest Home Page
 *   (http://harvest.cs.colorado.edu/) from the query page of any
 *   Broker you deploy, as well as in the query result pages.  These
 *   links are generated automatically by the standard Broker
 *   software distribution.
 *   
 *   The Harvest software is provided ``as is'', without express or
 *   implied warranty, and with no support nor obligation to assist
 *   in its use, correction, modification or enhancement.  We assume
 *   no liability with respect to the infringement of copyrights,
 *   trade secrets, or any patents, and are not responsible for
 *   consequential damages.  Proper use of the Harvest software is
 *   entirely the responsibility of the user.
 *  
 * DERIVATIVE WORKS
 *  
 *   Users may make derivative works from the Harvest software, subject 
 *   to the following constraints:
 *  
 *     - You must include the above copyright notice and these 
 *       accompanying paragraphs in all forms of derivative works, 
 *       and any documentation and other materials related to such 
 *       distribution and use acknowledge that the software was 
 *       developed at the above institutions.
 *  
 *     - You must notify IRTF-RD regarding your distribution of 
 *       the derivative work.
 *  
 *     - You must clearly notify users that your are distributing 
 *       a modified version and not the original Harvest software.
 *  
 *     - Any derivative product is also subject to these copyright 
 *       and use restrictions.
 *  
 *   Note that the Harvest software is NOT in the public domain.  We
 *   retain copyright, as specified above.
 *  
 * HISTORY OF FREE SOFTWARE STATUS
 *  
 *   Originally we required sites to license the software in cases
 *   where they were going to build commercial products/services
 *   around Harvest.  In June 1995 we changed this policy.  We now
 *   allow people to use the core Harvest software (the code found in
 *   the Harvest ``src/'' directory) for free.  We made this change
 *   in the interest of encouraging the widest possible deployment of
 *   the technology.  The Harvest software is really a reference
 *   implementation of a set of protocols and formats, some of which
 *   we intend to standardize.  We encourage commercial
 *   re-implementations of code complying to this set of standards.  
 */

#include "squid.h"

time_t squid_starttime = 0;
time_t next_cleaning = 0;
int theHttpConnection = -1;
int theInIcpConnection = -1;
int theOutIcpConnection = -1;
int do_reuse = 1;
int opt_unlink_on_reload = 0;
int opt_reload_hit_only = 0;	/* only UDP_HIT during store relaod */
int catch_signals = 1;
int opt_dns_tests = 1;
int opt_foreground_rebuild = 0;
int vhost_mode = 0;
int unbuffered_logs = 1;	/* debug and hierarhcy unbuffered by default */
int shutdown_pending = 0;	/* set by SIGTERM handler (shut_down()) */
int reread_pending = 0;		/* set by SIGHUP handler */
char version_string[] = SQUID_VERSION;
char appname[] = "squid";
char localhost[] = "127.0.0.1";
struct in_addr local_addr;

/* for error reporting from xmalloc and friends */
extern void (*failure_notify) _PARAMS((char *));

static int rotate_pending = 0;	/* set by SIGUSR1 handler */
static int httpPortNumOverride = 1;
static int icpPortNumOverride = 1;	/* Want to detect "-u 0" */
#if MALLOC_DBG
static int malloc_debug_level = 0;
#endif
static void rotate_logs _PARAMS((int));
static void reconfigure _PARAMS((int));

static void usage()
{
    fprintf(stderr, "\
Usage: %s [-hsvzCDFRUVY] [-f config-file] [-[au] port]\n\
       -a port   Specify ASCII port number (default: %d).\n\
       -f file   Use given config-file instead of\n\
                 %s\n\
       -h        Print help message.\n\
       -s        Enable logging to syslog.\n\
       -u port   Specify UDP port number (default: %d), disable with 0.\n\
       -v        Print version.\n\
       -z        Zap disk storage -- deletes all objects in disk cache.\n\
       -C        Do not catch fatal signals.\n\
       -D        Disable initial DNS tests.\n\
       -F        Foreground fast store rebuild.\n\
       -R        Do not set REUSEADDR on port.\n\
       -U        Unlink expired objects on reload.\n\
       -V        Virtual host httpd-accelerator.\n\
       -Y        Only return UDP_HIT or UDP_DENIED during fast store reload.\n",
	appname, CACHE_HTTP_PORT, DefaultConfigFile, CACHE_ICP_PORT);
    exit(1);
}

static void mainParseOptions(argc, argv)
     int argc;
     char *argv[];
{
    extern char *optarg;
    int c;

    while ((c = getopt(argc, argv, "CDFRUVYa:bf:hm:su:vz?")) != -1) {
	switch (c) {
	case 'C':
	    catch_signals = 0;
	    break;
	case 'D':
	    opt_dns_tests = 0;
	    break;
	case 'F':
	    opt_foreground_rebuild = 1;
	    break;
	case 'R':
	    do_reuse = 0;
	    break;
	case 'U':
	    opt_unlink_on_reload = 1;
	    break;
	case 'V':
	    vhost_mode = 1;
	    break;
	case 'Y':
	    opt_reload_hit_only = 1;
	    break;
	case 'a':
	    httpPortNumOverride = atoi(optarg);
	    break;
	case 'b':
	    unbuffered_logs = 0;
	    break;
	case 'f':
	    xfree(ConfigFile);
	    ConfigFile = xstrdup(optarg);
	    break;
	case 'h':
	    usage();
	    break;
	case 'm':
#if MALLOC_DBG
	    malloc_debug_level = atoi(optarg);
	    break;
#else
	    fatal("Need to add -DMALLOC_DBG when compiling to use -m option");
#endif
	case 's':
	    syslog_enable = 0;
	    break;
	case 'u':
	    icpPortNumOverride = atoi(optarg);
	    if (icpPortNumOverride < 0)
		icpPortNumOverride = 0;
	    break;
	case 'v':
	    printf("Squid Cache: Version %s\n", version_string);
	    exit(0);
	    /* NOTREACHED */
	case 'z':
	    zap_disk_store = 1;
	    break;
	case '?':
	default:
	    usage();
	    break;
	}
    }
}

void rotate_logs(sig)
     int sig;
{
    debug(21, 1, "rotate_logs: SIGUSR1 received.\n");
    rotate_pending = 1;
#if !HAVE_SIGACTION
    signal(sig, rotate_logs);
#endif
}

void reconfigure(sig)
     int sig;
{
    debug(21, 1, "reconfigure: SIGHUP received\n");
    debug(21, 1, "Waiting %d seconds for active connections to finish\n",
	getShutdownLifetime());
    reread_pending = 1;
#if !HAVE_SIGACTION
    signal(sig, reconfigure);
#endif
}

void shut_down(sig)
     int sig;
{
    debug(21, 1, "Preparing for shutdown after %d connections\n",
	ntcpconn + nudpconn);
    debug(21, 1, "Waiting %d seconds for active connections to finish\n",
	getShutdownLifetime());
    shutdown_pending = 1;
}

void serverConnectionsOpen()
{
    struct in_addr addr;
    u_short port;
    /* Get our real priviliges */

    /* Open server ports */
    enter_suid();
    theHttpConnection = comm_open(COMM_NONBLOCKING,
	getTcpIncomingAddr(),
	getHttpPortNum(),
	"HTTP Port");
    leave_suid();
    if (theHttpConnection < 0) {
	fatal("Cannot open HTTP Port");
    }
    fd_note(theHttpConnection, "HTTP socket");
    comm_listen(theHttpConnection);
    comm_set_select_handler(theHttpConnection,
	COMM_SELECT_READ,
	asciiHandleConn,
	0);
    debug(1, 1, "Accepting HTTP connections on FD %d.\n",
	theHttpConnection);

    if (!httpd_accel_mode || getAccelWithProxy()) {
	if ((port = getIcpPortNum()) > 0) {
	    theInIcpConnection = comm_open(COMM_NONBLOCKING | COMM_DGRAM,
		getUdpIncomingAddr(),
		port,
		"ICP Port");
	    if (theInIcpConnection < 0)
		fatal("Cannot open ICP Port");
	    fd_note(theInIcpConnection, "ICP socket");
	    comm_set_select_handler(theInIcpConnection,
		COMM_SELECT_READ,
		icpHandleUdp,
		0);
	    debug(1, 1, "Accepting ICP connections on FD %d.\n",
		theInIcpConnection);

	    if ((addr = getUdpOutgoingAddr()).s_addr != INADDR_NONE) {
		theOutIcpConnection = comm_open(COMM_NONBLOCKING | COMM_DGRAM,
		    addr,
		    port,
		    "ICP Port");
		if (theOutIcpConnection < 0)
		    fatal("Cannot open Outgoing ICP Port");
		comm_set_select_handler(theOutIcpConnection,
		    COMM_SELECT_READ,
		    icpHandleUdp,
		    0);
		debug(1, 1, "Accepting ICP connections on FD %d.\n",
		    theOutIcpConnection);
		fd_note(theOutIcpConnection, "Outgoing ICP socket");
		fd_note(theInIcpConnection, "Incoming ICP socket");
	    } else {
		theOutIcpConnection = theInIcpConnection;
	    }
	}
    }
}

void serverConnectionsClose()
{
    /* NOTE, this function will be called repeatedly while shutdown
     * is pending */
    if (theHttpConnection >= 0) {
	debug(21, 1, "FD %d Closing HTTP connection\n",
	    theHttpConnection);
	comm_close(theHttpConnection);
	comm_set_select_handler(theHttpConnection,
	    COMM_SELECT_READ,
	    NULL,
	    0);
	theHttpConnection = -1;
    }
    if (theInIcpConnection >= 0) {
	/* NOTE, don't close outgoing ICP connection, we need to write to
	 * it during shutdown */
	debug(21, 1, "FD %d Closing ICP connection\n",
	    theInIcpConnection);
	if (theInIcpConnection != theOutIcpConnection)
	    comm_close(theInIcpConnection);
	comm_set_select_handler(theInIcpConnection,
	    COMM_SELECT_READ,
	    NULL,
	    0);
	if (theInIcpConnection != theOutIcpConnection)
	    comm_set_select_handler(theOutIcpConnection,
		COMM_SELECT_READ,
		NULL,
		0);
	theInIcpConnection = -1;
    }
}

static void mainReinitialize()
{
    debug(1, 0, "Restarting Squid Cache (version %s)...\n", version_string);
    /* Already called serverConnectionsClose and ipcacheShutdownServers() */
    neighborsDestroy();

    parseConfigFile(ConfigFile);
    _db_init(getCacheLogFile(), getDebugOptions());
    neighbors_init();
    ipcacheOpenServers();
    serverConnectionsOpen();
    (void) ftpInitialize();
    if (theOutIcpConnection >= 0 && (!httpd_accel_mode || getAccelWithProxy()))
	neighbors_open(theOutIcpConnection);
    debug(1, 0, "Ready to serve requests.\n");
}

static void mainInitialize()
{
    static int first_time = 1;
    if (catch_signals) {
	squid_signal(SIGSEGV, death, SA_NODEFER | SA_RESETHAND);
	squid_signal(SIGBUS, death, SA_NODEFER | SA_RESETHAND);
    }
    squid_signal(SIGPIPE, SIG_IGN, SA_RESTART);
    squid_signal(SIGCHLD, sig_child, SA_NODEFER | SA_RESTART);

    if (ConfigFile == NULL)
	ConfigFile = xstrdup(DefaultConfigFile);
    parseConfigFile(ConfigFile);

    leave_suid();		/* Run as non privilegied user */

    if (httpPortNumOverride != 1)
	setHttpPortNum((u_short) httpPortNumOverride);
    if (icpPortNumOverride != 1)
	setIcpPortNum((u_short) icpPortNumOverride);

    _db_init(getCacheLogFile(), getDebugOptions());
    fdstat_open(fileno(debug_log), FD_LOG);
    fd_note(fileno(debug_log), getCacheLogFile());

    debug(1, 0, "Starting Squid Cache version %s for %s...\n",
	version_string,
	CONFIG_HOST_TYPE);
    debug(1, 1, "With %d file descriptors available\n", FD_SETSIZE);

    if (first_time) {
	disk_init();		/* disk_init must go before ipcache_init() */
	writePidFile();		/* write PID file */
    }
    ipcache_init();
    neighbors_init();
    (void) ftpInitialize();

#if MALLOC_DBG
    malloc_debug(0, malloc_debug_level);
#endif

    if (first_time) {
	first_time = 0;
	/* module initialization */
	urlInitialize();
	stat_init(&CacheInfo, getAccessLogFile());
	storeInit();
	stmemInit();

	if (getEffectiveUser()) {
	    /* we were probably started as root, so cd to a swap
	     * directory in case we dump core */
	    if (chdir(swappath(0)) < 0) {
		debug(1, 0, "%s: %s\n", swappath(0), xstrerror());
		fatal_dump("Cannot cd to swap directory?");
	    }
	}
	/* after this point we want to see the mallinfo() output */
	do_mallinfo = 1;
    }
    serverConnectionsOpen();
    if (theOutIcpConnection >= 0 && (!httpd_accel_mode || getAccelWithProxy()))
	neighbors_open(theOutIcpConnection);

    squid_signal(SIGUSR1, rotate_logs, SA_RESTART);
    squid_signal(SIGUSR2, sigusr2_handle, SA_RESTART);
    squid_signal(SIGHUP, reconfigure, SA_RESTART);
    squid_signal(SIGTERM, shut_down, SA_NODEFER | SA_RESETHAND | SA_RESTART);
    squid_signal(SIGINT, shut_down, SA_NODEFER | SA_RESETHAND | SA_RESTART);
    debug(1, 0, "Ready to serve requests.\n");
}


int main(argc, argv)
     int argc;
     char **argv;
{
    int errcount = 0;
    int n;			/* # of GC'd objects */
    time_t last_maintain = 0;
    time_t last_announce = 0;
    time_t loop_delay;

    memset(&local_addr, '\0', sizeof(struct in_addr));
    local_addr.s_addr = inet_addr(localhost);

    errorInitialize();

    squid_starttime = getCurrentTime();
    failure_notify = fatal_dump;

    mainParseOptions(argc, argv);

    setMaxFD();

    if (catch_signals)
	for (n = FD_SETSIZE; n > 2; n--)
	    close(n);

#if HAVE_MALLOPT
#ifdef M_GRAIN
    /* Round up all sizes to a multiple of this */
    mallopt(M_GRAIN, 16);
#endif
#ifdef M_MXFAST
    /* biggest size that is considered a small block */
    mallopt(M_MXFAST, 256);
#endif
#ifdef M_NBLKS
    /* allocate this many small blocks at once */
    mallopt(M_NLBLKS, 32);
#endif
#endif /* HAVE_MALLOPT */

    /*init comm module */
    comm_init();

    /* we have to init fdstat here. */
    fdstat_init(PREOPEN_FD);
    fdstat_open(0, FD_LOG);
    fdstat_open(1, FD_LOG);
    fdstat_open(2, FD_LOG);
    fd_note(0, "STDIN");
    fd_note(1, "STDOUT");
    fd_note(2, "STDERR");

    /* enable syslog by default */
    syslog_enable = 0;

    /* preinit for debug module */
    debug_log = stderr;
    hash_init(0);

    mainInitialize();

    /* main loop */
    if (getCleanRate() > 0)
	next_cleaning = time(NULL) + getCleanRate();
    for (;;) {
	loop_delay = (time_t) 10;
	/* maintain cache storage */
	if (squid_curtime > last_maintain) {
	    storeMaintainSwapSpace();
	    last_maintain = squid_curtime;
	}
	if (rotate_pending) {
	    ftpServerClose();
	    _db_rotate_log();	/* cache.log */
	    storeWriteCleanLog();
	    storeRotateLog();	/* store.log */
	    neighbors_rotate_log();	/* hierarchy.log */
	    stat_rotate_log();	/* access.log */
	    (void) ftpInitialize();
	    rotate_pending = 0;
	}
	/* do background processing */
	if (doBackgroundProcessing())
	    loop_delay = (time_t) 0;
	switch (comm_select(loop_delay, next_cleaning)) {
	case COMM_OK:
	    errcount = 0;	/* reset if successful */
	    break;
	case COMM_ERROR:
	    errcount++;
	    debug(1, 0, "Select loop Error. Retry %d\n", errcount);
	    if (errcount == 10)
		fatal_dump("Select Loop failed!");
	    break;
	case COMM_TIMEOUT:
	    if (getCleanRate() > 0 && squid_curtime >= next_cleaning) {
		debug(1, 1, "Performing a garbage collection...\n");
		n = storePurgeOld();
		debug(1, 1, "Garbage collection done, %d objects removed\n", n);
		next_cleaning = squid_curtime + getCleanRate();
	    }
	    if ((n = getAnnounceRate()) > 0) {
		if (squid_curtime > last_announce + n)
		    send_announce();
		last_announce = squid_curtime;
	    }
	    /* house keeping */
	    break;
	case COMM_SHUTDOWN:
	    /* delayed close so we can transmit while shutdown pending */
	    if (theOutIcpConnection > 0) {
		comm_close(theOutIcpConnection);
		theOutIcpConnection = -1;
	    }
	    if (shutdown_pending) {
		normal_shutdown();
		exit(0);
	    } else if (reread_pending) {
		mainReinitialize();
		reread_pending = 0;	/* reset */
	    } else {
		fatal_dump("MAIN: SHUTDOWN from comm_select, but nothing pending.");
	    }
	    break;
	default:
	    fatal_dump("MAIN: Internal error -- this should never happen.");
	    break;
	}
    }
    /* NOTREACHED */
    exit(0);
    return 0;
}

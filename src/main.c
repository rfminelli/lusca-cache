/* $Id$ */

/* DEBUG: Section 1             main: startup and main loop */

#include "squid.h"

time_t cached_starttime = 0;
time_t next_cleaning = 0;
int theAsciiConnection = -1;
int theUdpConnection = -1;
int do_reuse = 1;
int catch_signals = 1;
int do_dns_test = 1;
char *config_file = NULL;
int vhost_mode = 0;
int unbuffered_logs = 1;	/* debug and hierarhcy unbuffered by default */
int shutdown_pending = 0;	/* set by SIGTERM handler (shut_down()) */
int reread_pending = 0;		/* set by SIGHUP handler */

extern void (*failure_notify) ();	/* for error reporting from xmalloc */

static int asciiPortNumOverride = 0;
static int udpPortNumOverride = 0;
static int malloc_debug_level = 0;

static void usage()
{
    fprintf(stderr, "\
Usage: cached [-Rsehvz] [-f config-file] [-[apu] port]\n\
       -h        Print help message.\n\
       -s        Enable logging to syslog.\n\
       -v        Print version.\n\
       -z        Zap disk storage -- deletes all objects in disk cache.\n\
       -C        Do not catch fatal signals.\n\
       -D        Disable initial DNS tests.\n\
       -R        Do not set REUSEADDR on port.\n\
       -f file   Use given config-file instead of\n\
                 $HARVEST_HOME/lib/cached.conf.\n\
       -a port	 Specify ASCII port number (default: %d).\n\
       -u port	 Specify UDP port number (default: %d).\n",
	CACHE_HTTP_PORT, CACHE_ICP_PORT);
    exit(1);
}

static void mainParseOptions(argc, argv)
     int argc;
     char *argv[];
{
    extern char *optarg;
    int c;

    while ((c = getopt(argc, argv, "vCDRVbsif:a:p:u:m:zh?")) != -1) {
	switch (c) {
	case 'v':
	    printf("Harvest Cache: Version %s\n", SQUID_VERSION);
	    exit(0);
	    /* NOTREACHED */
	case 'b':
	    unbuffered_logs = 0;
	    break;
	case 'V':
	    vhost_mode = 1;
	    break;
	case 'C':
	    catch_signals = 0;
	    break;
	case 'D':
	    do_dns_test = 0;
	    break;
	case 's':
	    syslog_enable = 0;
	    break;
	    break;
	case 'R':
	    do_reuse = 0;
	    break;
	case 'f':
	    xfree(config_file);
	    config_file = xstrdup(optarg);
	    break;
	case 'a':
	    asciiPortNumOverride = atoi(optarg);
	    break;
	case 'u':
	    udpPortNumOverride = atoi(optarg);
	    break;
	case 'm':
	    malloc_debug_level = atoi(optarg);
	    break;
	case 'z':
	    zap_disk_store = 1;
	    break;
	case '?':
	case 'h':
	default:
	    usage();
	    break;
	}
    }
}

void serverConnectionsOpen()
{
    theAsciiConnection = comm_open(COMM_NONBLOCKING,
	getAsciiPortNum(),
	0,
	"Ascii Port");
    if (theAsciiConnection < 0) {
	fatal("Cannot open ascii Port");
    }
    fdstat_open(theAsciiConnection, Socket);
    fd_note(theAsciiConnection, "HTTP (Ascii) socket");
    comm_listen(theAsciiConnection);
    comm_set_select_handler(theAsciiConnection,
	COMM_SELECT_READ,
	asciiHandleConn,
	0);
    debug(1, 1, "Accepting HTTP (ASCII) connections on FD %d.\n",
	theAsciiConnection);

    if (!httpd_accel_mode || getAccelWithProxy()) {
	if (getUdpPortNum() > -1) {
	    theUdpConnection = comm_open(COMM_NONBLOCKING | COMM_DGRAM,
		getUdpPortNum(),
		0,
		"Ping Port");
	    if (theUdpConnection < 0)
		fatal("Cannot open UDP Port");
	    fdstat_open(theUdpConnection, Socket);
	    fd_note(theUdpConnection, "ICP (UDP) socket");
	    comm_set_select_handler(theUdpConnection,
		COMM_SELECT_READ,
		icpHandleUdp,
		0);
	    debug(1, 1, "Accepting ICP (UDP) connections on FD %d.\n",
		theUdpConnection);
	}
    }
}

void serverConnectionsClose()
{
    if (theAsciiConnection >= 0) {
	debug(21, 1, "FD %d Closing Ascii connection\n",
	    theAsciiConnection);
	fdstat_close(theAsciiConnection);
	comm_close(theAsciiConnection);
	comm_set_select_handler(theAsciiConnection,
	    COMM_SELECT_READ,
	    NULL,
	    0);
	theAsciiConnection = -1;
    }
    if (theUdpConnection >= 0) {
	debug(21, 1, "FD %d Closing Udp connection\n",
	    theUdpConnection);
	fdstat_close(theUdpConnection);
	comm_close(theUdpConnection);
	comm_set_select_handler(theUdpConnection,
	    COMM_SELECT_READ,
	    NULL,
	    0);
	theUdpConnection = -1;
    }
}

static void mainUninitialize()
{
    neighborsDestroy();
    serverConnectionsClose();
    /* ipcache_uninit() */
    /* neighbors_uninit(); */
}

static void mainInitialize()
{
    static int first_time = 1;
    parseConfigFile(config_file);

    neighbors_create();

    if (asciiPortNumOverride > 0)
	setAsciiPortNum(asciiPortNumOverride);
    if (udpPortNumOverride > 0)
	setUdpPortNum(udpPortNumOverride);

    _db_init(getCacheLogFile());
    fdstat_open(fileno(debug_log), LOG);
    fd_note(fileno(debug_log), getCacheLogFile());

    debug(1, 0, "Starting Harvest Cache (version %s)...\n", SQUID_VERSION);

    ipcache_init();
    neighbors_init();
    ftpInitialize();

#if defined(MALLOC_DBG)
    malloc_debug(0, malloc_debug_level);
#endif

    serverConnectionsOpen();

    /* do suid checking here */
    check_suid();

    /* Now that the fd's are open, initialize neighbor connections */
    if (theUdpConnection >= 0 && (!httpd_accel_mode || getAccelWithProxy()))
	neighbors_open(theUdpConnection);

    if (first_time) {
	first_time = 0;
	/* module initialization */
	disk_init();
	stat_init(&CacheInfo, getAccessLogFile());
	storeInit();
	stmemInit();
	writePidFile();

	/* after this point we want to see the mallinfo() output */
	do_mallinfo = 1;
    }
    debug(1, 0, "Ready to serve requests.\n");
}


int main(argc, argv)
     int argc;
     char **argv;
{
    int errcount = 0;
    int n;			/* # of GC'd objects */
    time_t last_maintain = 0;

    errorInitialize();

    cached_starttime = getCurrentTime();
    failure_notify = fatal_dump;

    mainParseOptions(argc, argv);

    setMaxFD();

    for (n = getMaxFD(); n > 2; n--)
	close(n);

#if HAVE_MALLOPT
    /* set malloc option */
    /* use small block algorithm for faster allocation */
    /* grain of small block */
    mallopt(M_GRAIN, 16);
    /* biggest size that is considered a small block */
    mallopt(M_MXFAST, 4096);
    /* number of holding small block */
    mallopt(M_NLBLKS, 100);
#endif

    /*init comm module */
    comm_init();

    /* we have to init fdstat here. */
    fdstat_init(PREOPEN_FD);
    fdstat_open(0, LOG);
    fdstat_open(1, LOG);
    fdstat_open(2, LOG);
    fd_note(0, "STDIN");
    fd_note(1, "STDOUT");
    fd_note(2, "STDERR");

    if (config_file == NULL)
	config_file = xstrdup(DEFAULT_CONFIG_FILE);

    /* enable syslog by default */
    syslog_enable = 0;

    /* preinit for debug module */
    debug_log = stderr;
    hash_init(0);

    if (catch_signals) {
	signal(SIGSEGV, death);
	signal(SIGBUS, death);
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, sig_child);
    signal(SIGHUP, rotate_logs);
    signal(SIGTERM, shut_down);
    signal(SIGINT, shut_down);

    mainInitialize();

    /* main loop */
    if (getCleanRate() > 0)
	next_cleaning = time(0L) + getCleanRate();
    while (1) {
	/* maintain cache storage */
	if (cached_curtime > last_maintain) {
	    storeMaintainSwapSpace();
	    last_maintain = cached_curtime;
	}
	switch (comm_select((long) 60, (long) 0, next_cleaning)) {
	case COMM_OK:
	    /* do nothing */
	    break;
	case COMM_ERROR:
	    errcount++;
	    debug(1, 0, "Select loop Error. Retry. %d\n", errcount);
	    if (errcount == 10)
		fatal_dump("Select Loop failed!");
	    break;
	case COMM_TIMEOUT:
	    /* this happens after 1 minute of idle time, or
	     * when next_cleaning has arrived */
	    /* garbage collection */
	    if (getCleanRate() > 0 && cached_curtime >= next_cleaning) {
		debug(1, 1, "Performing a garbage collection...\n");
		n = storePurgeOld();
		debug(1, 1, "Garbage collection done, %d objects removed\n", n);
		next_cleaning = cached_curtime + getCleanRate();
	    }
	    /* house keeping */
	    break;
	case COMM_SHUTDOWN:
	    if (shutdown_pending) {
		normal_shutdown();
		exit(0);
	    } else if (reread_pending) {
		mainUninitialize();
		mainInitialize();
		reread_pending = 0;	/* reset */
	    } else {
		fatal_dump("MAIN: SHUTDOWN from comm_select, but nothing pending.");
	    }
	default:
	    fatal_dump("MAIN: Internal error -- this should never happen.");
	    break;
	}
    }
    /* NOTREACHED */
    exit(0);
}

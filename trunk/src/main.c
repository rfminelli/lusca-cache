/* $Id$ */

#include "squid.h"


/* WRITE_PID_FILE - tries to write a cached.pid file on startup */
#ifndef WRITE_PID_FILE
#define WRITE_PID_FILE
#endif

time_t cached_starttime = 0;
time_t next_cleaning = 0;
int theAsciiConnection = -1;
int theBinaryConnection = -1;
int theUdpConnection = -1;
int do_reuse = 1;
int debug_level = 0;
int catch_signals = 1;
int do_dns_test = 1;
char *config_file = NULL;
int vhost_mode = 0;
int unbuffered_logs = 0;	/* debug and hierarhcy buffered by default */

extern void (*failure_notify) ();	/* for error reporting from xmalloc */
extern void hash_init _PARAMS((int));
extern int disk_init();
extern void stmemInit();
extern int storeMaintainSwapSpace();
extern void fatal_dump _PARAMS((char *));
extern void fatal _PARAMS((char *));
extern void kill_zombie();
extern int ftpInitialize _PARAMS((void));
extern int getMaxFD _PARAMS((void));

static int asciiPortNumOverride = 0;
static int binaryPortNumOverride = 0;
static int udpPortNumOverride = 0;

void raise_debug_lvl(), reset_debug_lvl();
void sig_child();

int main(argc, argv)
     int argc;
     char **argv;
{
    int c;
    int malloc_debug_level = 0;
    int debug_level_overwrite = 0;
    extern char *optarg;
    int errcount = 0;
    static int neighbors = 0;
    char *s = NULL;
    int n;			/* # of GC'd objects */
    time_t last_maintain = 0;

#ifdef WRITE_PID_FILE
    FILE *pid_fp = NULL;
    static char pidfn[MAXPATHLEN];
#endif

    cached_starttime = cached_curtime = time((time_t *) NULL);
    failure_notify = fatal_dump;

    for (n = getMaxFD(); n > 2; n--)
	close(n);

    /* try to use as many file descriptors as possible */
    /* System V uses RLIMIT_NOFILE and BSD uses RLIMIT_OFILE */
#if defined(HAVE_SETRLIMIT)
    {
	struct rlimit rl;

#if defined(RLIMIT_NOFILE)
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
	    perror("getrlimit: RLIMIT_NOFILE");
	} else {
	    rl.rlim_cur = rl.rlim_max;	/* set it to the max */
	    if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
		perror("setrlimit: RLIMIT_NOFILE");
	    }
	}
#elif defined(RLIMIT_OFILE)
	if (getrlimit(RLIMIT_OFILE, &rl) < 0) {
	    perror("getrlimit: RLIMIT_OFILE");
	} else {
	    rl.rlim_cur = rl.rlim_max;	/* set it to the max */
	    if (setrlimit(RLIMIT_OFILE, &rl) < 0) {
		perror("setrlimit: RLIMIT_OFILE");
	    }
	}
#endif
    }
#endif

#if USE_MALLOPT
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

#ifdef DAEMON
    if (daemonize()) {
	fprintf(stderr, "Error: couldn't create daemon process\n");
	exit(0);
    }
    /*  signal( SIGHUP, restart ); *//* restart if/when proc dies */
#endif /* DAEMON */

    /* we have to init fdstat here. */
    fdstat_init(PREOPEN_FD);
    fdstat_open(0, LOG);
    fdstat_open(1, LOG);
    fdstat_open(2, LOG);
    fd_note(0, "STDIN");
    fd_note(1, "STDOUT");
    fd_note(2, "STDERR");

    if ((s = getenv("HARVEST_HOME")) != NULL) {
	config_file = (char *) xcalloc(1, strlen(s) + 64);
	sprintf(config_file, "%s/lib/cached.conf", s);
    } else {
	config_file = xstrdup("/usr/local/harvest/lib/cached.conf");
    }

    /* enable syslog by default */
    syslog_enable = 1;
    /* disable stderr debug printout by default */
    stderr_enable = 0;
    /* preinit for debug module */
    debug_log = stderr;
    hash_init(0);

    while ((c = getopt(argc, argv, "vCDRVseif:a:p:u:d:m:zh?")) != -1)
	switch (c) {
	case 'v':
	    printf("Harvest Cache: Version %s\n", SQUID_VERSION);
	    exit(0);
	    /* NOTREACHED */
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
	case 'e':
	    stderr_enable = 1;
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
	case 'p':
	    binaryPortNumOverride = atoi(optarg);
	    break;
	case 'u':
	    udpPortNumOverride = atoi(optarg);
	    break;
	case 'd':
	    stderr_enable = 1;
	    debug_level_overwrite = 1;
	    debug_level = atoi(optarg);
	    unbuffered_logs = 1;
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
	    printf("\
Usage: cached [-Rsehvz] [-f config-file] [-d debug-level] [-[apu] port]\n\
       -e        Print messages to stderr.\n\
       -h        Print help message.\n\
       -s        Disable syslog output.\n\
       -v        Print version.\n\
       -z        Zap disk storage -- deletes all objects in disk cache.\n\
       -C        Do not catch fatal signals.\n\
       -D        Disable initial DNS tests.\n\
       -R        Do not set REUSEADDR on port.\n\
       -f file   Use given config-file instead of\n\
                 $HARVEST_HOME/lib/cached.conf.\n\
       -d level  Use given debug-level, prints messages to stderr.\n\
       -a port	 Specify ASCII port number (default: %d).\n\
       -u port	 Specify UDP port number (default: %d).\n",
		CACHE_HTTP_PORT, CACHE_ICP_PORT);

	    exit(1);
	    break;
	}

    if (catch_signals) {
	signal(SIGSEGV, death);
	signal(SIGBUS, death);
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, sig_child);
    signal(SIGHUP, rotate_logs);
    signal(SIGTERM, shut_down);
    signal(SIGINT, shut_down);

    parseConfigFile(config_file);

    if (!neighbors) {
	neighbors_create();
	++neighbors;
    };

    if (asciiPortNumOverride > 0)
	setAsciiPortNum(asciiPortNumOverride);
    if (binaryPortNumOverride > 0)
	setBinaryPortNum(binaryPortNumOverride);
    if (udpPortNumOverride > 0)
	setUdpPortNum(udpPortNumOverride);

    if (!debug_level_overwrite) {
	debug_level = getDebugLevel();
    }
    /* to toggle debugging */
#ifdef SIGUSR1
    signal(SIGUSR1, raise_debug_lvl);
#endif
#ifdef SIGUSR2
    signal(SIGUSR2, reset_debug_lvl);
#endif

#ifdef NO_LOGGING
    _db_init("cached", 0, getCacheLogFile());
#else
    _db_init("cached", debug_level, getCacheLogFile());
#endif
    fdstat_open(fileno(debug_log), LOG);
    fd_note(fileno(debug_log), getCacheLogFile());

    debug(0, "Starting Harvest Cache (version %s)...\n", SQUID_VERSION);

    /* init ipcache */
    ipcache_init();

    /* init neighbors */
    neighbors_init();

    ftpInitialize();


#if defined(MALLOC_DBG)
    malloc_debug(malloc_debug_level);
#endif

    theAsciiConnection = comm_open(COMM_NONBLOCKING,
	getAsciiPortNum(),
	0,
	"Ascii Port");
    if (theAsciiConnection < 0) {
	fatal("Cannot open ascii Port\n");
    }
    fdstat_open(theAsciiConnection, Socket);
    fd_note(theAsciiConnection, "HTTP (Ascii) socket");
    comm_listen(theAsciiConnection);
    comm_set_select_handler(theAsciiConnection,
	COMM_SELECT_READ,
	asciiHandleConn,
	0);
    debug(1, "Accepting HTTP (ASCII) connections on FD %d.\n",
	theAsciiConnection);

    if (!httpd_accel_mode || getAccelWithProxy()) {
#ifdef KEEP_BINARY_CONN
	theBinaryConnection = comm_open(COMM_NONBLOCKING,
	    binaryPortNum,
	    0,
	    "Binary Port");

	if (theBinaryConnection < 0) {
	    fatal("Cannot open Binary Port\n");
	}
	comm_listen(theBinaryConnection);
	comm_set_select_handler(theBinaryConnection,
	    COMM_SELECT_READ,
	    icpHandleTcp,
	    0);
	debug(1, "Binary connection opened on fd %d\n", theBinaryConnection);
#endif
	if (getUdpPortNum() > -1) {
	    theUdpConnection = comm_open(COMM_NONBLOCKING | COMM_DGRAM,
		getUdpPortNum(),
		0,
		"Ping Port");
	    if (theUdpConnection < 0)
		fatal("Cannot open UDP Port\n");
	    fdstat_open(theUdpConnection, Socket);
	    fd_note(theUdpConnection, "ICP (UDP) socket");
	    comm_set_select_handler(theUdpConnection,
		COMM_SELECT_READ,
		icpHandleUdp,
		0);
	    debug(1, "Accepting ICP (UDP) connections on FD %d.\n",
		theUdpConnection);
	}
    }
    if (theUdpConnection > 0) {
	/* Now that the fd's are open, initialize neighbor connections */
	if (!httpd_accel_mode || getAccelWithProxy()) {
	    neighbors_open(theUdpConnection);
	}
    }
    /* do suid checking here */
    check_suid();

    /* module initialization */
    disk_init();
    stat_init(&CacheInfo, getAccessLogFile());
    storeInit();
    stmemInit();

#ifdef WRITE_PID_FILE
    /* Try to write the pid to cached.pid in the same directory as
     * cached.conf */
    memset(pidfn, '\0', MAXPATHLEN);
    strcpy(pidfn, config_file);
    if ((s = strrchr(pidfn, '/')) != NULL)
	strcpy(s, "/cached.pid");
    else
	strcpy(pidfn, "/usr/local/harvest/lib/cached.pid");
    pid_fp = fopen(pidfn, "w");
    if (pid_fp != NULL) {
	fprintf(pid_fp, "%d\n", (int) getpid());
	fclose(pid_fp);
    }
#endif

    /* after this point we want to see the mallinfo() output */
    do_mallinfo = 1;
    debug(0, "Ready to serve requests.\n");

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
	    debug(0, "Select loop Error. Retry. %d\n", errcount);
	    if (errcount == 10)
		fatal_dump("Select Loop failed.!\n");
	    break;
	case COMM_TIMEOUT:
	    /* this happens after 1 minute of idle time, or
	     * when next_cleaning has arrived */
	    /* garbage collection */
	    if (getCleanRate() > 0 && cached_curtime >= next_cleaning) {
		debug(1, "Performing a garbage collection...\n");
		n = storePurgeOld();
		debug(1, "Garbage collection done, %d objects removed\n", n);
		next_cleaning = cached_curtime + getCleanRate();
	    }
	    /* house keeping */
	    break;
	default:
	    debug(0, "MAIN: Internal error -- this should never happen.\n");
	    break;
	}
    }
    /* NOTREACHED */
    exit(0);
}

void raise_debug_lvl()
{
    extern int _db_level;
    _db_level = 10;

#if defined(_SQUID_SYSV_SIGNALS_) && defined(SIGUSR1)
    signal(SIGUSR1, raise_debug_lvl);
#endif
}

void reset_debug_lvl()
{
    extern int _db_level;
    _db_level = debug_level;

#if defined(_SQUID_SYSV_SIGNALS_) && defined(SIGUSR2)
    signal(SIGUSR2, reset_debug_lvl);
#endif
}

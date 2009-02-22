#include "include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
  
#include "include/Array.h"
#include "include/Stack.h"
#include "include/util.h"
#include "libcore/valgrind.h"
#include "libcore/varargs.h"
#include "libcore/debug.h"
#include "libcore/kb.h"
#include "libcore/gb.h"
#include "libcore/tools.h"

#include "libmem/MemPool.h"
#include "libmem/MemBufs.h"
#include "libmem/MemBuf.h"
  
#include "libcb/cbdata.h"

#include "libsqinet/inet_legacy.h"
#include "libsqinet/sqinet.h"

#include "libiapp/iapp_ssl.h"
#include "libiapp/fd_types.h"
#include "libiapp/comm_types.h"
#include "libiapp/comm.h"
#include "libiapp/pconn_hist.h"
#include "libiapp/signals.h"
#include "libiapp/mainloop.h"

#include "libsqbgp/bgp_core.h"
#include "libsqbgp/bgp_packet.h"
#include "libsqbgp/bgp_rib.h"

sqaddr_t dest;

int
main(int argc, const char *argv[])
{
	int fd;
	const char *host;
	short port;
	u_short as_num;
	u_short hold_time;

	if (argc < 4) {
		printf("Usage: %s <host> <port> <asnum> <hold-time>\n", argv[0]);
		exit(1);
	}

	iapp_init();
	squid_signal(SIGPIPE, SIG_IGN, SA_RESTART);

	_db_init("ALL,1");
	_db_set_stderr_debug(1);

	/* Create BGP FSM */

	/* Setup outgoing socket */

	/* Kick off BGP FSM with given FD */

	while (1) {
		iapp_runonce(60000);
	}

	exit(0);
}


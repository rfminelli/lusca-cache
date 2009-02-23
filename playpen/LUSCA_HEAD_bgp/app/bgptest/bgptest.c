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
#include "include/radix.h"
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
#include "libiapp/event.h"

#include "libsqbgp/radix.h"
#include "libsqbgp/bgp_packet.h"
#include "libsqbgp/bgp_rib.h"
#include "libsqbgp/bgp_core.h"
#include "libsqbgp/bgp_conn.h"

int
main(int argc, const char *argv[])
{
        struct in_addr bgp_id;
	bgp_conn_t *bc;
#if 0
	if (argc < 4) {
		printf("Usage: %s <host> <port> <asnum> <hold-time>\n", argv[0]);
		exit(1);
	}
#endif

	iapp_init();
	squid_signal(SIGPIPE, SIG_IGN, SA_RESTART);

	_db_init("ALL,1 85,99");
	_db_set_stderr_debug(99);
 
	bc = bgp_conn_create();
	inet_aton("216.12.163.53", &bgp_id);
	bgp_set_lcl(&bc->bi, bgp_id, 65535, 120);
	bgp_set_rem(&bc->bi, 38620);
        inet_aton("216.12.163.51", &bc->rem_ip);
	bc->rem_port = 179;

        /* connect to bgp thing */
	bgp_conn_begin_connect(bc);

	while (1) {
		iapp_runonce(60000);
	}

	exit(0);
}


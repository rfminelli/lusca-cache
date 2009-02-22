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
        int fd, r;
        struct sockaddr_in sa;
        struct in_addr bgp_id;
	bgp_instance_t bi;

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

 
 
        bzero(&sa, sizeof(sa));
        inet_aton("216.12.163.51", &sa.sin_addr);
        inet_aton("216.12.163.53", &bgp_id);
        sa.sin_port = htons(179);
        sa.sin_len = sizeof(struct sockaddr_in);
        sa.sin_family = AF_INET;

	bgp_create_instance(&bi);
	bgp_set_lcl(&bi, bgp_id, 65535, 120);
	bgp_set_rem(&bi, 38620);

        /* connect to bgp thing */
 
        while (1) {
        	fd = socket(AF_INET, SOCK_STREAM, 0);
        	assert(fd != -1);
        	r = connect(fd, (struct sockaddr *) &sa, sizeof(sa));
        	r = bgp_send_hello(fd, 65535, 120, bgp_id);
		if (r > 0)
			while (r > 0)
				r = bgp_read(&bi, fd);
		bgp_close(&bi);
		close(fd);
		debug(85, 1) ("sleeping for 15 seconds..\n");
		sleep(15);
        }

#if 0
	while (1) {
		iapp_runonce(60000);
	}
#endif

	exit(0);
}


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
        char buf[4096];
        int bufofs = 0;
        int i, len;

#if 0
	if (argc < 4) {
		printf("Usage: %s <host> <port> <asnum> <hold-time>\n", argv[0]);
		exit(1);
	}
#endif

	iapp_init();
	squid_signal(SIGPIPE, SIG_IGN, SA_RESTART);

	_db_init("ALL,1");
	_db_set_stderr_debug(1);

 
        fd = socket(AF_INET, SOCK_STREAM, 0);
        assert(fd != -1);
 
        /* connect to bgp thing */
        bzero(&sa, sizeof(sa));
        inet_aton("216.12.163.51", &sa.sin_addr);
        inet_aton("216.12.163.53", &bgp_id);
        sa.sin_port = htons(179);
        sa.sin_len = sizeof(struct sockaddr_in);
        sa.sin_family = AF_INET;
        r = connect(fd, (struct sockaddr *) &sa, sizeof(sa));
        assert(r > -1);
 
        /* Now, loop over and read messages */
        /* We'll eventually have to uhm, speak BGP.. */
        r = bgp_send_hello(fd, 65535, 120, bgp_id);
 
        printf("ready to read stuff\n");
 
        while (1) {
                bzero(buf + bufofs, sizeof(buf) - bufofs);
                /* XXX should check there's space in the buffer first! */
                printf("main: space in buf is %d bytes\n", (int) sizeof(buf) - bufofs);
                len = read(fd, buf + bufofs, sizeof(buf) - bufofs);
                assert(len > 0);
                bufofs += len;
                printf("read: %d bytes; bufsize is now %d\n", len, bufofs);
                i = 0;

                /* loop over; try to handle partial messages */
                while (i < len) {
                        printf("looping..\n");
                        /* Is there enough data here? */
                        if (! bgp_msg_complete(buf + i, bufofs - i)) {
                                printf("main: incomplete packet\n");
                                break;
                        }
                        r = bgp_decode_message(fd, buf + i, bufofs - i);
                        assert(r > 0);
                        i += r;
                        printf("main: pkt was %d bytes, i is now %d\n", r, i);
                }
                /* "consume" the rest of the buffer */
                memmove(buf, buf + i, sizeof(buf) - i);
                bufofs -= i;
                printf("consumed %d bytes; bufsize is now %d\n", i, bufofs);
        }

#if 0
	while (1) {
		iapp_runonce(60000);
	}
#endif

	exit(0);
}


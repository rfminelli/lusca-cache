#include "include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
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

#include "libiapp/iapp_ssl.h"
#include "libiapp/globals.h"
#include "libiapp/comm.h"
#include "libiapp/pconn_hist.h"
#include "libiapp/mainloop.h"

#include "tunnel.h"

static void
acceptSock(int sfd, void *d)
{
	int fd;
	struct sockaddr_in peer, me, dest;

	bzero(&me, sizeof(me));
	bzero(&peer, sizeof(peer));
	fd = comm_accept(sfd, &peer, &me);
	debug(1, 1) ("acceptSock: FD %d: new socket!\n", fd);

	/* Create tunnel */
	safe_inet_addr("192.168.1.31", &dest.sin_addr);
	dest.sin_port = htons(80);
	dest.sin_family = AF_INET;
	dest.sin_len = sizeof(struct sockaddr_in);
	sslStart(fd, dest);
	/* register for another pass */
	commSetSelect(sfd, COMM_SELECT_READ, acceptSock, NULL, 0);
}

int
main(int argc, const char *argv[])
{
	int fd;
	struct sockaddr_in s;

	iapp_init();

	_db_init("ALL,99");
	_db_set_stderr_debug(99);

	bzero(&s.sin_addr, sizeof(s.sin_addr));
	s.sin_port = htons(8080);

	fd = comm_open(SOCK_STREAM, IPPROTO_TCP, s.sin_addr, 8080, COMM_NONBLOCKING, "HTTP Socket");
	printf("new fd: %d\n", fd);
	assert(fd > 0);
	comm_listen(fd);
	commSetSelect(fd, COMM_SELECT_READ, acceptSock, NULL, 0);

	printf("beginning!\n");
	while (1) {
		printf("runonce!!\n");
		iapp_runonce(60000);
	}

	exit(0);
}


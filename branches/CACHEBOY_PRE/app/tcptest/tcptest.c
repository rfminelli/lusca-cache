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

static void
acceptSock(int sfd, void *d)
{
	int fd;
	struct sockaddr_in peer, me;


	bzero(&me, sizeof(me));
	bzero(&peer, sizeof(peer));
	fd = comm_accept(sfd, &peer, &me);
	debug(1, 1) ("acceptSock: FD %d: new socket!\n", fd);
	printf("foo! %d\n", fd);
	commSetSelect(sfd, COMM_SELECT_READ, acceptSock, NULL, 0);
}

int
main(int argc, const char *argv[])
{
	int fd;
	struct sockaddr_in s;

	iapp_init();

	bzero(&s.sin_addr, sizeof(s.sin_addr));
	s.sin_port = htons(8080);

	debugLevels[1] = 99;

	fd = comm_open(SOCK_STREAM, IPPROTO_TCP, s.sin_addr, 8080, COMM_NONBLOCKING, "HTTP Socket");
	printf("new fd: %d\n", fd);
	assert(fd > 0);
	comm_listen(fd);
	commSetSelect(fd, COMM_SELECT_READ, acceptSock, NULL, 0);

	printf("beginning!\n");
	while (1) {
		printf("runonce!!\n");
		iapp_runonce(1000);
	}

	exit(0);
}


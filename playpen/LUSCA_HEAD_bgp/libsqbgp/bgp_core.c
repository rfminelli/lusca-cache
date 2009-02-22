#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/util.h"
#include "../libcore/tools.h"
#include "../libsqdebug/debug.h"

#include "../libsqinet/sqinet.h"


#include "bgp_packet.h"
#include "bgp_rib.h"
#include "bgp_core.h"


#if 0
int
main(int argc, const char *argv[])
{
	int fd, r;
	struct sockaddr_in sa;
	struct in_addr bgp_id;
	char buf[4096];
	int bufofs = 0;
	int i, len;

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

	exit(0);
}
#endif

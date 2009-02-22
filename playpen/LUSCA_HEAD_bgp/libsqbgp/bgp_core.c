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


void
bgp_create_instance(bgp_instance_t *bi)
{
	bzero(bi, sizeof(*bi));
	bi->state = BGP_IDLE;
}

void
bgp_destroy_instance(bgp_instance_t *bi)
{
	/* Free RIB entries and RIB tree */
	/* Ensure AS path entries are gone, complain for leftovers! */
	/* Free AS path hash */
	bzero(bi, sizeof(*bi));
}

void
bgp_set_lcl(bgp_instance_t *bi, struct in_addr bgp_id, u_short asn, u_short hold_time)
{
	memcpy(&bi->lcl.bgp_id, &bgp_id, sizeof(bgp_id));
	bi->lcl.asn = asn;
	bi->lcl.hold_time = hold_time;
}

void
bgp_set_rem(bgp_instance_t *bi, u_short asn)
{
	bi->rem.asn = asn;
	bi->rem.hold_time = -1;
}

/*
 * Move to ACTIVE phase. In this phase, BGP is trying to actively
 * establish a connection.
 *
 * This code doesn't do the networking stuff - instead, the owner of this BGP
 * connection state will do the networking stuff and call bgp_close() or
 * bgp_open().
 */
void
bgp_active(bgp_instance_t *bi)
{

}

/*
 * Read some data from the given file descriptor (using the read() syscall for now!)
 * and update the BGP state machine. The caller should check for retval <= 0 and reset
 * the socket and FSM as appropriate.
 */
int
bgp_read(bgp_instance_t *bi, int fd)
{
	int len, i, r;

	/* Append data to buffer */
	bzero(bi->recv.buf + bi->recv.bufofs, BGP_RECV_BUF - bi->recv.bufofs);
	debug(85, 1) ("main: space in buf is %d bytes\n", (int) BGP_RECV_BUF - bi->recv.bufofs);

	len = read(fd, bi->recv.buf + bi->recv.bufofs, BGP_RECV_BUF - bi->recv.bufofs);
	if (len <= 0)
		return len;

	bi->recv.bufofs += len;
	debug(85, 2) ("read: %d bytes; bufsize is now %d\n", len, bi->recv.bufofs);
	i = 0;
	/* loop over; try to handle partial messages */
	while (i < len) {
		debug(85, 1) ("looping..\n");
		/* Is there enough data here? */
		if (! bgp_msg_complete(bi->recv.buf + i, bi->recv.bufofs - i)) {
			debug(85, 1) ("main: incomplete packet\n");
			break;
		}
		r = bgp_decode_message(fd, bi->recv.buf + i, bi->recv.bufofs - i);
		assert(r > 0);
		i += r;
		debug(85, 1) ("main: pkt was %d bytes, i is now %d\n", r, i);
	}
	/* "consume" the rest of the buffer */
	memmove(bi->recv.buf, bi->recv.buf + i, BGP_RECV_BUF - i);
	bi->recv.bufofs -= i;
	debug(85, 1) ("consumed %d bytes; bufsize is now %d\n", i, bi->recv.bufofs);
	return len;
}

/*
 * Move connection to IDLE state. Destroy any AS path / prefix entries.
 */
void
bgp_close(bgp_instance_t *bi)
{
	bi->state = BGP_IDLE;
	/* free prefixes */
	/* ensure no as path entries exist in the hash! */
}


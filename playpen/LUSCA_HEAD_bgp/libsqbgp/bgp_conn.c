#include "../include/config.h"

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
  
#include "../include/Array.h"
#include "../include/Stack.h"
#include "../include/util.h"
#include "../include/radix.h"
#include "../libcore/valgrind.h"
#include "../libcore/varargs.h"
#include "../libcore/debug.h"
#include "../libcore/kb.h"
#include "../libcore/gb.h"
#include "../libcore/tools.h"

#include "../libmem/MemPool.h"
#include "../libmem/MemBufs.h"
#include "../libmem/MemBuf.h"
  
#include "../libcb/cbdata.h"

#include "../libsqinet/inet_legacy.h"
#include "../libsqinet/sqinet.h"

#include "../libiapp/iapp_ssl.h"
#include "../libiapp/fd_types.h"
#include "../libiapp/comm_types.h"
#include "../libiapp/comm.h"
#include "../libiapp/pconn_hist.h"
#include "../libiapp/signals.h"
#include "../libiapp/mainloop.h"
#include "../libiapp/event.h"

#include "radix.h"
#include "bgp_packet.h"
#include "bgp_rib.h"
#include "bgp_core.h"
#include "bgp_conn.h"

CBDATA_TYPE(bgp_conn_t);
bgp_conn_t *
bgp_conn_create(void)
{
	bgp_conn_t *bc;

	CBDATA_INIT_TYPE(bgp_conn_t);
	bc = cbdataAlloc(bgp_conn_t);
	bc->fd = -1;
	bgp_create_instance(&bc->bi);
	return bc;
}

void
bgp_conn_connect_wakeup(void *data)
{
	bgp_conn_t *bc = data;
	bgp_conn_begin_connect(bc);
}

void
bgp_conn_connect_sleep(bgp_conn_t *bc)
{
	eventAdd("bgp retry", bgp_conn_connect_wakeup, bc, 10.0, 1);
}

void
bgp_conn_send_keepalive(void *data)
{
	bgp_conn_t *bc = data;
	int r;
	int hold_timer;

	/* Do we have valid hold timers for both sides of the connection? If so, schedule the recurring keepalive msg */
	/* XXX should really check the state here! */

	/* Hold timer of 0 means "don't ever send a keepalive" (RFC4271, 4.4.) */
	hold_timer = bgp_get_holdtimer(&bc->bi);
	if (hold_timer <= 0)
		return;

	r = bgp_send_keepalive(bc->fd);
	if (r <= 0) {
		bgp_conn_close_and_restart(bc);
		return;
	}
	//eventAdd("bgp keepalive", bgp_conn_send_keepalive, bc, (hold_timer + 4) / 2, 0);
	eventAdd("bgp keepalive", bgp_conn_send_keepalive, bc, 5.0, 0);

}

void
bgp_conn_destroy(bgp_conn_t *bc)
{
	debug(85, 1) ("bgp_conn_destroy: %p: shutting down BGP session\n", bc);
	eventDelete(bgp_conn_connect_wakeup, bc);
	if (bc->fd > -1) {
		comm_close(bc->fd);
		bc->fd = -1;
	}
	bgp_close(&bc->bi);
	cbdataFree(bc);
}

void
bgp_conn_close_and_restart(bgp_conn_t *bc)
{
	if (bc->fd > -1) {
		comm_close(bc->fd);
		bc->fd = -1;
	}
	bgp_close(&bc->bi);
	if (eventFind(bgp_conn_connect_wakeup, bc))
		eventDelete(bgp_conn_connect_wakeup, bc);
	if (eventFind(bgp_conn_send_keepalive, bc))
	eventDelete(bgp_conn_send_keepalive, bc);
	bgp_conn_connect_sleep(bc);
}

static void
bgp_conn_handle_read(int fd, void *data)
{
	bgp_conn_t *bc = data;
	int r;

	debug(85, 2) ("bgp_conn_handle_read: %p: FD %d: state %d: READY\n", bc, fd, bc->bi.state);
	r = bgp_read(&bc->bi, fd);
	if (r <= 0) {
		bgp_conn_close_and_restart(bc);
		return;
	}
	commSetSelect(fd, COMM_SELECT_READ, bgp_conn_handle_read, data, 0);
	/* Have we tried sending a keepalive yet? Then try */
	if (! bc->keepalive_event) {
		bc->keepalive_event = 1;
		bgp_conn_send_keepalive(bc);
	}
}

static void
bgp_conn_handle_write(int fd, void *data)
{
	bgp_conn_t *bc = data;
	int r;

	debug(85, 2) ("bgp_conn_handle_write: %p: FD %d: state %d: READY\n", bc, fd, bc->bi.state);
	switch(bc->bi.state) {
		case BGP_OPEN:
			/* Send OPEN; set state to OPEN_CONFIRM */
        		r = bgp_send_hello(fd, bc->bi.lcl.asn, bc->bi.lcl.hold_timer, bc->bi.lcl.bgp_id);
			if (r <= 0) {
				bgp_conn_close_and_restart(bc);
				return;
			}
			bgp_openconfirm(&bc->bi);
			commSetSelect(fd, COMM_SELECT_READ, bgp_conn_handle_read, data, 0);
			break;
		default:
			assert(1==0);
	}
}

void
bgp_conn_connect_done(int fd, int status, void *data)
{
	bgp_conn_t *bc = data;

	if (status != COMM_OK) {
		bgp_conn_close_and_restart(bc);
		return;
	}
	/* XXX set timeout? */

	/* XXX set bgp instance state to open; get ready to send OPEN message */
	bgp_open(&bc->bi);

	/* Register for write readiness - send OPEN */
	commSetSelect(fd, COMM_SELECT_WRITE, bgp_conn_handle_write, data, 0);
}

void
bgp_conn_begin_connect(bgp_conn_t *bc)
{
	sqaddr_t peer;

	if (bc->fd > -1) {
		comm_close(bc->fd);
		bgp_close(&bc->bi);
		bc->fd = -1;
	}

	debug(85, 1) ("bgp_conn_begin_connect: %p: beginning connect to %s:%d\n", bc, inet_ntoa(bc->rem_ip), bc->rem_port);
	bc->fd = comm_open(SOCK_STREAM, IPPROTO_TCP, bc->bi.lcl.bgp_id, 0,
	    COMM_NONBLOCKING, COMM_TOS_DEFAULT, "BGP connection");
	assert(bc->fd > 0);
	sqinet_init(&peer);
	sqinet_set_v4_inaddr(&peer, &bc->rem_ip);
	sqinet_set_v4_port(&peer, bc->rem_port, SQADDR_ASSERT_IS_V4);

	comm_connect_begin(bc->fd, &peer, bgp_conn_connect_done, bc);
	sqinet_done(&peer);
}


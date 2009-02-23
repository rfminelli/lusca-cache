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
#include "../include/radix.h"
#include "../libcore/tools.h"
#include "../libsqdebug/debug.h"

#include "../libsqinet/sqinet.h"


#include "bgp_packet.h"
#include "bgp_rib.h"
#include "bgp_core.h"

struct _bgp_update_state {
	u_int8_t aspath_type;
	u_int8_t aspath_len;
	u_short *aspaths;
	int origin;
	struct in_addr *nlri;
	struct in_addr *nexthop;
	struct in_addr *withdraw;
};
typedef struct _bgp_update_state bgp_update_state_t;

int
bgp_msg_len(const char *buf, int len)
{
	u_int16_t msg_len;

	if (len < 19)
		return -1;

	msg_len = ntohs(* (u_int16_t *) (buf + 16));
	return msg_len;
}

int
bgp_msg_type(const char *buf, int len)
{
	u_int8_t type;

	if (len < 19)
		return -1;

	type = * (u_int8_t *) (buf + 18);
	return type;
}

int
bgp_msg_isvalid(const char *buf, int len)
{
	return -1;
}

int
bgp_msg_complete(const char *buf, int len)
{
	int type;
	int msg_len;

	type = bgp_msg_type(buf, len);
	msg_len = bgp_msg_len(buf, len);
	debug(85, 2) ("bgp_msg_complete: type %d, pktlen %d, msglen %d\n", type, len, msg_len);

	if (type < 0)
		return 0;
	if (msg_len > len)
		return 0;
	return 1;
}

/*
 * XXX absolutely hacky!
 */
int
bgp_send_hello(bgp_instance_t *bi, int fd, unsigned short asnum, short hold_time, struct in_addr bgp_id)
{
	char send_buf[128];
	char *p = send_buf;
	u_int16_t t;
	u_int16_t len;
	u_int8_t t8;
	int pkt_len;
	
	/* marker */
	memset(p, 255, 16);
	p += 16;

	/* length - fill out later! */
	p += 2;

	/* type */
	t8 = 1;	/* open message */
	memcpy(p, &t8, sizeof(t8));
	p += 1;

	/* now, hello */
	t8 = 4; /* bgp4 */
	memcpy(p, &t8, sizeof(t8));
	p += 1;

	/* as number */
	t = htons(asnum);
	memcpy(p, &t, sizeof(t));
	p += 2;

	/* hold time */
	t = htons(hold_time);
	memcpy(p, &t, sizeof(t));
	p += 2;

	/* bgp identifier */
	memcpy(p, &bgp_id.s_addr, sizeof(bgp_id.s_addr));
	p += 4;

	/* optional parameter */
	t8 = 0;
	memcpy(p, &t8, sizeof(t8));
	p += 1;

	pkt_len = p - send_buf;
	debug(85, 2) ("OPEN: len: %d\n", pkt_len);
	len = htons(p - send_buf);
	p = send_buf + 16;
	memcpy(p, &len, sizeof(len));

	return (write(fd, send_buf, pkt_len) == pkt_len);
}

int
bgp_send_keepalive(bgp_instance_t *bi, int fd)
{
	char send_buf[128];
	char *p = send_buf;
	u_int8_t t8;
	int pkt_len;
	u_int16_t len;

	/* marker */
	memset(p, 255, 16);
	p += 16;

	/* length - fill out later! */
	p += 2;

	/* type */
	t8 = 4;	/* open message */
	memcpy(p, &t8, sizeof(t8));
	p += 1;

	pkt_len = p - send_buf;
	debug(85, 2) ("bgp_send_keepalive: KEEPALIVE: len: %d\n", pkt_len);
	len = htons(p - send_buf);
	p = send_buf + 16;
	memcpy(p, &len, sizeof(len));

	return (write(fd, send_buf, pkt_len) == pkt_len);


}

int
bgp_handle_notification(bgp_instance_t *bi, int fd, const char *buf, int len)
{
	u_int8_t err_code;
	u_int8_t err_subcode;
	u_int16_t err_data;

	err_code = * (u_int8_t *) buf;
	err_subcode = * (u_int8_t *) (buf + 2);
	err_data = ntohs(* (u_int8_t *) (buf + 4));
	debug(85, 2) ("bgp_handle_notification: err %d, subcode %d, data %d\n", err_code, err_subcode, err_data);
	return 1;
}

int
bgp_handle_open(bgp_instance_t *bi, int fd, const char *buf, int len)
{
	int parm_len;

	/* XXX should ensure we have enough space! */
	/* XXX should check version!? */

	bi->rem.version = * (u_int8_t *) buf;
	bi->rem.asn = ntohs(* (u_int16_t *) (buf + 1));
	bi->rem.hold_timer = ntohs(* (u_int16_t *) (buf + 3));
	memcpy(&bi->rem.bgp_id, buf + 5, 4);

	parm_len = * (u_int8_t *) (buf + 9);
	/* XXX don't bother decoding the OPEN parameters for now! */

	debug(85, 2) ("bgp_handle_open: got version %d, AS %d, timer %d, parm_len %d\n", bi->rem.version, bi->rem.asn, bi->rem.hold_timer, parm_len);

	/* Queue a keepalive message */
	bgp_send_keepalive(bi, fd);

	return 1;
}

int
bgp_handle_update_withdraw(bgp_instance_t *bi, bgp_update_state_t *us, const char *buf, int len)
{
	u_int8_t pl, netmask;
	int i = 0, j = 0;

	if (len == 0)
		return 1;

	/* Pre-calculate the maximum number of possible entries in this! */
	assert(us->withdraw == NULL);
	us->withdraw = xcalloc(len, sizeof(struct in_addr));

	debug(85, 2) ("  bgp_handle_update_withdraw: len %d\n", len);
	while (i < len) {
		/* The "length" is the number of bits which are "valid" .. */
		netmask = (* (u_int8_t *) (buf + i));
		if (netmask == 0)
			pl = 0;
		else
			pl = ((netmask - 1) / 8) + 1;
		i++;
		debug(85, 2) ("  bgp_handle_update_withdraw: netmask %d; len %d\n", netmask, pl);
		/* XXX bounds check? */
		memcpy(&us->withdraw[j], buf + i, pl);
		debug(85, 2) ("  bgp_handle_update_withdraw: prefix %s/%d\n", inet_ntoa(us->withdraw[j]), netmask);
		i += pl;
		j++;
	}
	return 1;
}

int
bgp_handle_update_pathattrib_origin(bgp_instance_t *bi, bgp_update_state_t *us, const char *buf, int len)
{
	if (len < 1)
		return 0;
	us->origin = * (u_int8_t *) buf;
	debug(85, 2) ("  bgp_handle_update_pathattrib_origin: origin id %d\n", us->origin);
	return 1;
}

int
bgp_handle_update_pathattrib_aspath(bgp_instance_t *bi, bgp_update_state_t *us, const char *buf, int len)
{
	int i, j = 0;

	if (len < 2)
		return 0;

	assert(us->aspaths == NULL);
	us->aspath_type = * (u_int8_t *) (buf);
	us->aspath_len = * (u_int8_t *) (buf + 1);
	us->aspaths = xcalloc(len / 2, sizeof(int));

	/* XXX well, the length should be verified / used / bounds checked? */

	debug(85, 2) ("  bgp_handle_update_pathattrib_aspath:");
	for (i = 2; i < len; i += 2) {
		us->aspaths[j] = ntohs(* (u_int16_t *) (buf + i));
		debug(85, 2) (" %d", us->aspaths[j]);
		j++;
	}
	debug(85, 2) ("\n");

	return 1;
}

int
bgp_handle_update_pathattrib(bgp_instance_t *bi, bgp_update_state_t *us, const char *buf, int len)
{
	int i = 0;
	u_int8_t a_flags, a_type;
	int a_len;

	/* Iterate over the buffer, pulling out <type, length, value> fields */
	/* XXX should bounds check some more! */
	debug(85, 2) ("bgp_handle_update_pathattrib: BEGIN\n");
	while (i < len) {
		a_flags = * (u_int8_t *) (buf + i);
		a_type = * (u_int8_t *) (buf + i + 1);
		i += 2;
		/* Length is either 8 or 16 bits; encoded by the extended length bit */
		if (a_flags & 0x10) {
			a_len = ntohs(* (u_int16_t *) (buf + i));
			i += 2;
		} else {
			a_len =  * (u_int8_t *) (buf + i);
			i += 1;
		}
		debug(85, 2) ("  bgp_handle_update_pathattrib: flags %x, type %x, len %d\n", a_flags, a_type, a_len);

		switch (a_type) {
			case 1:	/* origin */
				if (bgp_handle_update_pathattrib_origin(bi, us, buf + i, a_len) < 0)
					return 0;
				break;
			case 2: /* as path */
				if (bgp_handle_update_pathattrib_aspath(bi, us, buf + i, a_len) < 0)
					return 0;
				break;
			default:
				debug(85, 2) ("  bgp_handle_path_attrib: don't know type %d\n", a_type);
				break;
		}

		i += a_len;
	}
	debug(85, 2) ("bgp_handle_update_pathattrib: DONE\n");
	return 1;
}

int
bgp_handle_update_nlri(bgp_instance_t *bi, bgp_update_state_t *us, const char *buf, int len)
{
	struct in_addr pf;
	u_int8_t pl, netmask;
	int i = 0;

	if (len == 0)
		return 1;

	debug(85, 2) ("  bgp_handle_update_nlri: len %d\n", len);
	while (i < len) {
		bzero(&pf, sizeof(pf));
		/* The "length" is the number of bits which are "valid" .. */
		netmask = (* (u_int8_t *) (buf + i));
		if (netmask == 0)
			pl = 0;
		else
			pl = ((netmask - 1) / 8) + 1;
		i++;
		debug(85, 2) ("  bgp_handle_update_nlri: netmask %d; len %d\n", netmask, pl);
		/* XXX bounds check? */
		memcpy(&pf, buf + i, pl);
		debug(85, 2) ("  bgp_handle_update_nlri: prefix %s/%d\n", inet_ntoa(pf), netmask);
		bgp_rib_add_net(&bi->rn, pf, netmask);
		i += pl;
	}
	return 1;
}

int
bgp_handle_update(bgp_instance_t *bi, int fd, const char *buf, int len)
{
	int rc = 0;
	u_int16_t withdraw_route_len;
	u_int16_t path_attrib_len;
	bgp_update_state_t us;

	bzero(&us, sizeof(us));

	withdraw_route_len = ntohs(* (u_int16_t *) buf);
	path_attrib_len = ntohs(* (u_int16_t *) (buf + withdraw_route_len + 2));

	debug(85, 2) ("bgp_handle_update: UPDATE pktlen %d: withdraw_route_len %d; path attrib len %d\n",
	   len, withdraw_route_len, path_attrib_len);

	if (! bgp_handle_update_withdraw(bi, &us, buf + 2, withdraw_route_len)) {
		rc = 0; goto finish;
	}
	if (! bgp_handle_update_pathattrib(bi, &us, buf + 2 + withdraw_route_len + 2, path_attrib_len)) {
		rc = 0; goto finish;
	}
	if (! bgp_handle_update_nlri(bi, &us, buf + 2 + withdraw_route_len + 2 + path_attrib_len, len - (2 + withdraw_route_len + 2 + path_attrib_len))) {
		rc = 0; goto finish;
	}

	/* Now, we need to poke the RIB with our saved info */

	rc = 1;
finish:
	/* free said saved info */
	safe_free(us.withdraw);
	safe_free(us.nlri);
	safe_free(us.aspaths);

	return rc;
}

int
bgp_handle_keepalive(int fd, const char *buf, int len)
{
	debug(85, 2) ("bgp_handle_keepalive: KEEPALIVE RECEIVED\n");
	return 1;
}

int
bgp_decode_message(bgp_instance_t *bi, int fd, const const char *buf, int len)
{
	int r;

	u_int8_t type;
	u_int16_t pkt_len;
	/* XXX should check the marker is 16 bytes */
	/* XXX should make sure there's enough bytes in the msg! */

	pkt_len = ntohs(* (u_int16_t *) (buf + 16));
	type = * (u_int8_t *) (buf + 18);
	debug(85, 2) ("bgp_decode_message: type %d; len %d\n", type, pkt_len);
	switch 	(type) {
		case 1:		/* OPEN */
			r = bgp_handle_open(bi, fd, buf + 19, pkt_len - 19);
			break;
		case 2:		/* UPDATE */
			r = bgp_handle_update(bi, fd, buf + 19, pkt_len - 19);
			break;
		case 3:		/* NOTIFICATION */
			r = bgp_handle_notification(bi, fd, buf + 19, pkt_len - 19);
			break;
		case 4:		/* KEEPALIVE */
			r = bgp_handle_keepalive(fd, buf + 19, pkt_len - 19);
			break;
		default:
			debug(85, 2) ("bgp_decode_message: unknown message type: %d\n", type);
			exit(1);
	}

	return pkt_len;
}

#include "squid.h"

/* Currently Harvest cached-2.x uses ICP_VERSION_3 */
void
icpHandleIcpV3(int fd, struct sockaddr_in from, char *buf, int len)
{
    icp_common_t header;
    icp_common_t *reply;
    StoreEntry *entry = NULL;
    char *url = NULL;
    const cache_key *key;
    request_t *icp_request = NULL;
    int allow = 0;
    aclCheck_t checklist;
    method_t method;
    xmemcpy(&header, buf, sizeof(icp_common_t));
    /*
     * Only these fields need to be converted
     */
    header.length = ntohs(header.length);
    header.reqnum = ntohl(header.reqnum);
    header.flags = ntohl(header.flags);
    header.pad = ntohl(header.pad);

    method = header.reqnum >> 24;
    /* Squid-1.1 doesn't use the "method bits" for METHOD_GET */
    if (METHOD_NONE == method || METHOD_ENUM_END <= method)
	method = METHOD_GET;
    switch (header.opcode) {
    case ICP_QUERY:
	/* We have a valid packet */
	url = buf + sizeof(icp_common_t) + sizeof(u_num32);
	if ((icp_request = urlParse(method, url)) == NULL) {
	    reply = icpCreateMessage(ICP_ERR, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_INVALID, PROTO_NONE);
	    break;
	}
	checklist.src_addr = from.sin_addr;
	checklist.request = icp_request;
	allow = aclCheckFast(Config.accessList.icp, &checklist);
	if (!allow) {
	    debug(12, 2) ("icpHandleIcpV3: Access Denied for %s by %s.\n",
		inet_ntoa(from.sin_addr), AclMatchedName);
	    if (clientdbCutoffDenied(from.sin_addr)) {
		/*
		 * count this DENIED query in the clientdb, even though
		 * we're not sending an ICP reply...
		 */
		clientdbUpdate(from.sin_addr, LOG_UDP_DENIED, Config.Port.icp, 0);
	    } else {
		reply = icpCreateMessage(ICP_DENIED, 0, url, header.reqnum, 0);
		icpUdpSend(fd, &from, reply, LOG_UDP_DENIED, icp_request->protocol);
	    }
	    break;
	}
	/* The peer is allowed to use this cache */
	key = storeKeyPublic(url, method);
	entry = storeGet(key);
	debug(12, 5) ("icpHandleIcpV3: OPCODE %s\n",
	    icp_opcode_str[header.opcode]);
	if (icpCheckUdpHit(entry, icp_request)) {
	    reply = icpCreateMessage(ICP_HIT, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_HIT, icp_request->protocol);
	    break;
	}
	/* if store is rebuilding, return a UDP_HIT, but not a MISS */
	if (opt_reload_hit_only && store_rebuilding) {
	    reply = icpCreateMessage(ICP_MISS_NOFETCH, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_MISS_NOFETCH, icp_request->protocol);
	} else if (hit_only_mode_until > squid_curtime) {
	    reply = icpCreateMessage(ICP_MISS_NOFETCH, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_MISS_NOFETCH, icp_request->protocol);
	} else {
	    reply = icpCreateMessage(ICP_MISS, 0, url, header.reqnum, 0);
	    icpUdpSend(fd, &from, reply, LOG_UDP_MISS, icp_request->protocol);
	}
	break;

    case ICP_HIT:
    case ICP_SECHO:
    case ICP_DECHO:
    case ICP_MISS:
    case ICP_DENIED:
    case ICP_MISS_NOFETCH:
	if (neighbors_do_private_keys && header.reqnum == 0) {
	    debug(12, 0) ("icpHandleIcpV3: Neighbor %s returned reqnum = 0\n",
		inet_ntoa(from.sin_addr));
	    debug(12, 0) ("icpHandleIcpV3: Disabling use of private keys\n");
	    neighbors_do_private_keys = 0;
	}
	url = buf + sizeof(icp_common_t);
	debug(12, 3) ("icpHandleIcpV3: %s from %s for '%s'\n",
	    icp_opcode_str[header.opcode],
	    inet_ntoa(from.sin_addr),
	    url);
	if (neighbors_do_private_keys && header.reqnum)
	    key = storeKeyPrivate(url, method, header.reqnum);
	else
	    key = storeKeyPublic(url, method);
	/* call neighborsUdpAck even if ping_status != PING_WAITING */
	neighborsUdpAck(key, &header, &from);
	break;

    case ICP_INVALID:
    case ICP_ERR:
	break;

    default:
	debug(12, 0) ("icpHandleIcpV3: UNKNOWN OPCODE: %d from %s\n",
	    header.opcode, inet_ntoa(from.sin_addr));
	break;
    }
    if (icp_request) {
	stringClean(&icp_request->urlpath);
	memFree(MEM_REQUEST_T, icp_request);
    }
}

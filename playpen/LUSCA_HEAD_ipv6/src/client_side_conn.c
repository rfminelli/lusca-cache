#include "squid.h"
#include "client_db.h"

#include "client_side_conn.h"

static int clientside_num_conns = 0;

CBDATA_TYPE(ConnStateData);

/* This is a handler normally called by comm_close() */
static void
connStateFree(int fd, void *data)
{
    ConnStateData *connState = data;
    dlink_node *n;
    clientHttpRequest *http;
    debug(33, 3) ("connStateFree: FD %d\n", fd);
    assert(connState != NULL);
    clientdbEstablished6(&connState->peer2, -1);	/* decrement */
    n = connState->reqs.head;
    while (n != NULL) {
	http = n->data;
	n = n->next;
	assert(http->conn == connState);
	httpRequestFree(http);
    }
    if (connState->auth_user_request)
	authenticateAuthUserRequestUnlock(connState->auth_user_request);
    connState->auth_user_request = NULL;
    authenticateOnCloseConnection(connState);
    memFreeBuf(connState->in.size, connState->in.buf);
    pconnHistCount(0, connState->nrequests);
    if (connState->pinning.fd >= 0)
	comm_close(connState->pinning.fd);
    sqinet_done(&connState->me2);
    sqinet_done(&connState->peer2);
    sqinet_done(&connState->log_addr2);
    cbdataUnlock(connState->port);
    cbdataFree(connState);
    clientside_num_conns--;
#ifdef _SQUID_LINUX_
    /* prevent those nasty RST packets */
    {
	char buf[SQUID_TCP_SO_RCVBUF];
	while (FD_READ_METHOD(fd, buf, SQUID_TCP_SO_RCVBUF) > 0);
    }
#endif
}

ConnStateData *
connStateCreate(int fd, sqaddr_t *peer, sqaddr_t *me)
{
        ConnStateData *connState = NULL;
	sqaddr_t m;

        CBDATA_INIT_TYPE(ConnStateData);
        connState = cbdataAlloc(ConnStateData);
        clientside_num_conns++;
	sqinet_init(&connState->me2);
	sqinet_init(&connState->peer2);
	sqinet_init(&connState->log_addr2);

	sqinet_copy(&connState->peer2, peer);
	sqinet_copy(&connState->log_addr2, peer);

	if (sqinet_get_family(&connState->log_addr2) == AF_INET) {
		sqinet_init(&m);
		sqinet_set_v4_inaddr(&m, &Config.Addrs.client_netmask_v4);
		sqinet_mask_addr(&connState->log_addr2, &m);
		sqinet_done(&m);
	} else {
		sqinet_mask_addr(&connState->log_addr2, &Config.Addrs.client_netmask_v6);
	}

	sqinet_copy(&connState->me2, me);

        connState->fd = fd;
        connState->pinning.fd = -1;
        connState->in.buf = memAllocBuf(CLIENT_REQ_BUF_SZ, &connState->in.size);
        comm_add_close_handler(fd, connStateFree, connState);

        return connState;
}

int
connStateGetCount(void)
{
        return clientside_num_conns;
}

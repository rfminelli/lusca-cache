
#include "squid.h"

static void clientRedirectDone _PARAMS((void *data, char *result));

static int clientLookupDstIPDone(fd, hp, data)
     int fd;
     struct hostent *hp;
     void *data;
{
    icpStateData *icpState = data;
    debug(33, 5, "clientLookupDstIPDone: FD %d, '%s'\n",
	fd,
	icpState->url);
    icpState->aclChecklist->need &= ~(1 << ACL_DST_IP);
    icpState->aclChecklist->pend &= ~(1 << ACL_DST_IP);
    if (hp == NULL) {
	debug(33, 5, "clientLookupDstIPDone: Unknown host %s\n",
	    icpState->request->host);
	icpState->aclChecklist->dst_addr.s_addr = INADDR_NONE;
    } else {
	xmemcpy(&icpState->aclChecklist->dst_addr.s_addr,
	    *(hp->h_addr_list),
	    hp->h_length);
	debug(33, 5, "clientLookupDstIPDone: %s is %s\n",
	    icpState->request->host,
	    inet_ntoa(icpState->aclChecklist->dst_addr));
    }
    clientAccessCheck(icpState, icpState->aclHandler);
    return 1;
}

static void clientLookupSrcFQDNDone(fd, fqdn, data)
     int fd;
     char *fqdn;
     void *data;
{
    icpStateData *icpState = data;
    debug(33, 5, "clientLookupSrcFQDNDone: FD %d, '%s', FQDN %s\n",
	fd,
	icpState->url,
	fqdn ? fqdn : "NULL");
    icpState->aclChecklist->need &= ~(1 << ACL_SRC_DOMAIN);
    icpState->aclChecklist->pend &= ~(1 << ACL_SRC_DOMAIN);
    clientAccessCheck(icpState, icpState->aclHandler);
}

static void clientLookupIdentDone(data)
     void *data;
{
}

void clientAccessCheck(icpState, handler)
     icpStateData *icpState;
     void (*handler) _PARAMS((icpStateData *, int));
{
    int answer = 1;
    request_t *r = icpState->request;
    aclCheck_t *ch = NULL;
    if (icpState->aclChecklist == NULL) {
	icpState->aclChecklist = xcalloc(1, sizeof(aclCheck_t));
	icpState->aclChecklist->src_addr = icpState->peer.sin_addr;
	icpState->aclChecklist->request = requestLink(icpState->request);
    }
    ch = icpState->aclChecklist;
    icpState->aclHandler = handler;
    if (ch->pend) {
	debug(33, 1, "clientAccessCheck: ACL's still pending: %x\n",
	    ch->pend);
	return;
    }
    if (httpd_accel_mode && !getAccelWithProxy() && r->protocol != PROTO_CACHEOBJ) {
	/* this cache is an httpd accelerator ONLY */
	if (!BIT_TEST(icpState->flags, REQ_ACCEL))
	    answer = 0;
    } else {
	answer = aclCheck(HTTPAccessList, ch);
	if (ch->need) {
	    if (ch->need & (1 << ACL_DST_IP)) {
		ipcache_nbgethostbyname(icpState->request->host,
		    icpState->fd,
		    clientLookupDstIPDone,
		    icpState);
		ch->pend |= (1 << ACL_DST_IP);
	    } else if (ch->need & (1 << ACL_SRC_DOMAIN)) {
		fqdncache_nbgethostbyaddr(icpState->peer.sin_addr,
		    icpState->fd,
		    clientLookupSrcFQDNDone,
		    icpState);
		ch->pend |= (1 << ACL_SRC_DOMAIN);
	    }
	    return;
	}
    }
    requestUnlink(icpState->aclChecklist->request);
    safe_free(icpState->aclChecklist);
    icpState->aclHandler = NULL;
    (*handler) (icpState, answer);
}

void clientAccessCheckDone(icpState, answer)
     icpStateData *icpState;
     int answer;
{
    int fd = icpState->fd;
    char *buf = NULL;
    debug(33, 5, "clientAccessCheckDone: '%s' answer=%d\n", icpState->url, answer);
    if (answer) {
	urlCanonical(icpState->request, icpState->url);
	redirectStart(icpState->url, fd, clientRedirectDone, icpState);
    } else {
	debug(33, 5, "Access Denied: %s\n", icpState->url);
	buf = access_denied_msg(icpState->http_code = 400,
	    icpState->method,
	    icpState->url,
	    fd_table[fd].ipaddr);
	icpSendERROR(fd, LOG_TCP_DENIED, buf, icpState, 403);
    }
}

static void clientRedirectDone(data, result)
     void *data;
     char *result;
{
    icpStateData *icpState = data;
    int fd = icpState->fd;
    debug(33, 5, "clientRedirectDone: '%s' result=%s\n", icpState->url,
	result ? result : "NULL");
    if (result) {
	safe_free(icpState->url);
	icpState->url = xstrdup(result);
	urlCanonical(icpState->request, icpState->url);
    }
    icpParseRequestHeaders(icpState);
    fd_note(fd, icpState->url);
    comm_set_select_handler(fd,
	COMM_SELECT_READ,
	(PF) icpDetectClientClose,
	(void *) icpState);
    icp_hit_or_miss(fd, icpState);
}

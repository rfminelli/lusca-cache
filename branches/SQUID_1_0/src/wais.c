/* $Id$ */

/*
 * DEBUG: Section 24          wais
 */

#include "squid.h"

#define  WAIS_DELETE_GAP  (64*1024)

typedef struct _waisdata {
    StoreEntry *entry;
    method_t method;
    char *relayhost;
    int relayport;
    char *mime_hdr;
    char request[MAX_URL];
} WAISData;

static int waisStateFree(fd, waisState)
     int fd;
     WAISData *waisState;
{
    if (waisState == NULL)
	return 1;
    storeUnlockObject(waisState->entry);
    xfree(waisState);
    return 0;
}

/* This will be called when timeout on read. */
static void waisReadReplyTimeout(fd, data)
     int fd;
     WAISData *data;
{
    StoreEntry *entry = NULL;

    entry = data->entry;
    debug(24, 4, "waisReadReplyTimeout: Timeout on %d\n url: %s\n", fd, entry->url);
    squid_error_entry(entry, ERR_READ_TIMEOUT, NULL);
    comm_set_select_handler(fd, COMM_SELECT_READ, 0, 0);
    comm_close(fd);
}

/* This will be called when socket lifetime is expired. */
void waisLifetimeExpire(fd, data)
     int fd;
     WAISData *data;
{
    StoreEntry *entry = NULL;

    entry = data->entry;
    debug(24, 4, "waisLifeTimeExpire: FD %d: <URL:%s>\n", fd, entry->url);
    squid_error_entry(entry, ERR_LIFETIME_EXP, NULL);
    comm_set_select_handler(fd, COMM_SELECT_READ | COMM_SELECT_WRITE, 0, 0);
    comm_close(fd);
}




/* This will be called when data is ready to be read from fd.  Read until
 * error or connection closed. */
void waisReadReply(fd, data)
     int fd;
     WAISData *data;
{
    static char buf[4096];
    int len;
    StoreEntry *entry = NULL;

    entry = data->entry;
    if (entry->flag & DELETE_BEHIND) {
	if (storeClientWaiting(entry)) {
	    /* check if we want to defer reading */
	    if ((entry->mem_obj->e_current_len -
		    entry->mem_obj->e_lowest_offset) > WAIS_DELETE_GAP) {
		debug(24, 3, "waisReadReply: Read deferred for Object: %s\n",
		    entry->url);
		debug(24, 3, "                Current Gap: %d bytes\n",
		    entry->mem_obj->e_current_len -
		    entry->mem_obj->e_lowest_offset);
		/* reschedule, so it will automatically reactivated
		 * when Gap is big enough. */
		comm_set_select_handler(fd,
		    COMM_SELECT_READ,
		    (PF) waisReadReply,
		    (void *) data);
		/* don't install read handler while we're above the gap */
		comm_set_select_handler_plus_timeout(fd,
		    COMM_SELECT_TIMEOUT,
		    (PF) NULL,
		    (void *) NULL,
		    (time_t) 0);
		/* dont try reading again for a while */
		comm_set_stall(fd, getStallDelay());
		return;
	    }
	} else {
	    /* we can terminate connection right now */
	    squid_error_entry(entry, ERR_NO_CLIENTS_BIG_OBJ, NULL);
	    comm_close(fd);
	    return;
	}
    }
    len = read(fd, buf, 4096);
    debug(24, 5, "waisReadReply - fd: %d read len:%d\n", fd, len);

    if (len < 0) {
	debug(24, 1, "waisReadReply: FD %d: read failure: %s.\n", xstrerror());
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    /* reinstall handlers */
	    /* XXX This may loop forever */
	    comm_set_select_handler(fd, COMM_SELECT_READ,
		(PF) waisReadReply, (void *) data);
	    comm_set_select_handler_plus_timeout(fd, COMM_SELECT_TIMEOUT,
		(PF) waisReadReplyTimeout, (void *) data, getReadTimeout());
	} else {
	    BIT_RESET(entry->flag, CACHABLE);
	    storeReleaseRequest(entry);
	    squid_error_entry(entry, ERR_READ_ERROR, xstrerror());
	    comm_close(fd);
	}
    } else if (len == 0 && entry->mem_obj->e_current_len == 0) {
	squid_error_entry(entry,
	    ERR_ZERO_SIZE_OBJECT,
	    errno ? xstrerror() : NULL);
	comm_close(fd);
    } else if (len == 0) {
	/* Connection closed; retrieval done. */
	entry->expires = squid_curtime;
	storeComplete(entry);
	comm_close(fd);
    } else if (((entry->mem_obj->e_current_len + len) > getWAISMax()) &&
	!(entry->flag & DELETE_BEHIND)) {
	/*  accept data, but start to delete behind it */
	storeStartDeleteBehind(entry);
	storeAppend(entry, buf, len);
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) waisReadReply,
	    (void *) data);
	comm_set_select_handler_plus_timeout(fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) waisReadReplyTimeout,
	    (void *) data,
	    getReadTimeout());
    } else {
	storeAppend(entry, buf, len);
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) waisReadReply,
	    (void *) data);
	comm_set_select_handler_plus_timeout(fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) waisReadReplyTimeout,
	    (void *) data,
	    getReadTimeout());
    }
}

/* This will be called when request write is complete. Schedule read of
 * reply. */
void waisSendComplete(fd, buf, size, errflag, data)
     int fd;
     char *buf;
     int size;
     int errflag;
     WAISData *data;
{
    StoreEntry *entry = NULL;
    entry = data->entry;
    debug(24, 5, "waisSendComplete - fd: %d size: %d errflag: %d\n",
	fd, size, errflag);
    if (errflag) {
	squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	comm_close(fd);
    } else {
	/* Schedule read reply. */
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) waisReadReply,
	    (void *) data);
	comm_set_select_handler_plus_timeout(fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) waisReadReplyTimeout,
	    (void *) data,
	    getReadTimeout());
    }
    safe_free(buf);		/* Allocated by waisSendRequest. */
}

/* This will be called when connect completes. Write request. */
void waisSendRequest(fd, data)
     int fd;
     WAISData *data;
{
    int len = strlen(data->request) + 4;
    char *buf = NULL;
    char *Method = RequestMethodStr[data->method];

    debug(24, 5, "waisSendRequest - fd: %d\n", fd);

    if (Method)
	len += strlen(Method);
    if (data->mime_hdr)
	len += strlen(data->mime_hdr);

    buf = xcalloc(1, len + 1);

    if (data->mime_hdr)
	sprintf(buf, "%s %s %s\r\n", Method, data->request,
	    data->mime_hdr);
    else
	sprintf(buf, "%s %s\r\n", Method, data->request);
    debug(24, 6, "waisSendRequest - buf:%s\n", buf);
    icpWrite(fd,
	buf,
	len,
	30,
	waisSendComplete,
	(void *) data);
    if (BIT_TEST(data->entry->flag, CACHABLE))
	storeSetPublicKey(data->entry);		/* Make it public */
}

int waisStart(unusedfd, url, method, mime_hdr, entry)
     int unusedfd;
     char *url;
     method_t method;
     char *mime_hdr;
     StoreEntry *entry;
{
    /* Create state structure. */
    int sock, status;
    WAISData *data = NULL;

    debug(24, 3, "waisStart: \"%s %s\"\n",
	RequestMethodStr[method], url);
    debug(24, 4, "            header: %s\n", mime_hdr);

    if (!getWaisRelayHost()) {
	debug(24, 0, "waisStart: Failed because no relay host defined!\n");
	squid_error_entry(entry, ERR_NO_RELAY, NULL);
	return COMM_ERROR;
    }
    /* Create socket. */
    sock = comm_open(COMM_NONBLOCKING, 0, url);
    if (sock == COMM_ERROR) {
	debug(24, 4, "waisStart: Failed because we're out of sockets.\n");
	squid_error_entry(entry, ERR_NO_FDS, xstrerror());
	return COMM_ERROR;
    }
    data = xcalloc(1, sizeof(WAISData));
    storeLockObject(data->entry = entry, NULL, NULL);
    data->method = method;
    data->relayhost = getWaisRelayHost();
    data->relayport = getWaisRelayPort();
    data->mime_hdr = mime_hdr;
    strncpy(data->request, url, MAX_URL);
    comm_set_select_handler(sock,
	COMM_SELECT_CLOSE,
	(PF) waisStateFree,
	(void *) data);

    /* check if IP is already in cache. It must be. 
     * It should be done before this route is called. 
     * Otherwise, we cannot check return code for connect. */
    if (!ipcache_gethostbyname(data->relayhost)) {
	debug(24, 4, "waisstart: Called without IP entry in ipcache. OR lookup failed.\n");
	squid_error_entry(entry, ERR_DNS_FAIL, dns_error_message);
	comm_close(sock);
	return COMM_ERROR;
    }
    /* Open connection. */
    if ((status = comm_connect(sock, data->relayhost, data->relayport))) {
	if (status != EINPROGRESS) {
	    squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	    comm_close(sock);
	    return COMM_ERROR;
	} else {
	    debug(24, 5, "waisStart: FD %d EINPROGRESS\n", sock);
	}
    }
    /* Install connection complete handler. */
    comm_set_select_handler(sock, COMM_SELECT_LIFETIME,
	(PF) waisLifetimeExpire, (void *) data);
    comm_set_select_handler(sock, COMM_SELECT_WRITE,
	(PF) waisSendRequest, (void *) data);
    return COMM_OK;
}

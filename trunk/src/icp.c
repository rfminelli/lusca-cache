/* $Id$ */

/*
 * DEBUG: Section 12          icp:
 */

#include "squid.h"

extern int errno;
extern unsigned long nconn;
extern int vhost_mode;

static struct in_addr tmp_in_addr;
static char *crlf = "\r\n";

static char *log_tags[] =
{
    "LOG_TAG_MIN",
    "TCP_HIT",
    "TCP_MISS",
    "TCP_EXPIRED",
    "TCP_BLOCK",
    "TCP_DENIED",
    "UDP_HIT",
    "UDP_MISS",
    "UDP_DENIED",
    "ERR_READ_TIMEOUT",
    "ERR_LIFETIME_EXP",
    "ERR_NO_CLIENTS_BIG_OBJ",
    "ERR_READ_ERROR",
    "ERR_CLIENT_ABORT",
    "ERR_CONNECT_FAIL",
    "ERR_INVALID_URL",
    "ERR_NO_FDS",
    "ERR_DNS_FAIL",
    "ERR_NOT_IMPLEMENTED",
    "ERR_CANNOT_FETCH",
    "ERR_NO_RELAY",
    "ERR_DISK_IO",
    "ERR_URL_BLOCKED"
};

typedef struct iwd {
    icp_common_t header;	/* Allows access to previous header */
    u_num32 query_host;
    char *url;
    char *type;			/* GET, POST, ... */
    int type_id;
    char *mime_hdr;		/* Mime header */
    int html_request;
    StoreEntry *entry;
    long offset;
    int bytes_needed;		/*  Used for content_length */
    int log_type;
    struct sockaddr_in peer;
    struct sockaddr_in me;
    int accel_request;
    char *ptr_to_4k_page;
    char *buf;
} icpStateData;

static icpUdpData *UdpQueue = NULL;
static icpUdpData *tail = NULL;
#define ICP_MAX_UDP_SIZE 4096
#define ICP_SENDMOREDATA_BUF SM_PAGE_SIZE

#if !defined(UDP_HIT_THRESH)
#define UDP_HIT_THRESH 300
#endif

typedef void (*complete_handler) _PARAMS((int fd, char *buf, int size, int errflag, caddr_t data));
typedef struct ireadd {
    int fd;
    char *buf;
    long size;
    long offset;
    int timeout;		/* XXX Not used at present. */
    time_t time;
    complete_handler handler;
    caddr_t client_data;
} icpReadWriteData;

/* Local functions */
static void icpHandleStore _PARAMS((int, StoreEntry *, icpStateData *));
static void icpHandleStoreComplete _PARAMS((int, char *, int, int, icpStateData *));
static int icpProcessMISS _PARAMS((int, icpStateData *));
static void CheckQuickAbort _PARAMS((icpStateData *));
static icpUdpData *AppendUdp _PARAMS((icpUdpData *, icpUdpData *));

static void icpFreeBufOrPage(icpState)
     icpStateData *icpState;
{
    if (icpState->ptr_to_4k_page && icpState->buf)
	/* XXX should this be fatal? -DW */
	debug(12, 0, "Shouldn't have both a 4k ptr and a string\n");
    if (icpState->ptr_to_4k_page)
	put_free_4k_page(icpState->ptr_to_4k_page);
    else
	safe_free(icpState->buf);
}


static void icpCloseAndFree(fd, icpState, line)
     int fd;
     icpStateData *icpState;
     int line;			/* __LINE__ number of caller */
{
    int size = 0;
    if (fd > 0)
	comm_close(fd);
    if (!icpState) {
	sprintf(tmp_error_buf, "icpCloseAndFree: Called with NULL icpState from %s line %d", __FILE__, line);
	fatal_dump(tmp_error_buf);
    }
    debug(12, 1, "icpCloseAndFree: entry=%p\n", icpState->entry);
    if (icpState->entry)
	size = icpState->entry->mem_obj->e_current_len;
    debug(12, 1, "icpCloseAndFree: size=%d\n", size);
    CacheInfo->log_append(CacheInfo,
	icpState->url,
	inet_ntoa(icpState->peer.sin_addr),
	size,
	log_tags[icpState->log_type],
	icpState->type ? icpState->type : "UNKNOWN");
    safe_free(icpState->url);
    safe_free(icpState->type);
    safe_free(icpState->mime_hdr);
    safe_free(icpState);
}

/* Read from FD. */
int icpHandleRead(fd, rw_state_machine)
     int fd;
     icpReadWriteData *rw_state_machine;
{
    int len = read(fd, rw_state_machine->buf + rw_state_machine->offset,
	rw_state_machine->size - rw_state_machine->offset);

    if (len <= 0) {
	switch (errno) {
#if EAGAIN != EWOULDBLOCK
	case EAGAIN:
#endif
	case EWOULDBLOCK:
	    /* reschedule self */
	    comm_set_select_handler(fd, COMM_SELECT_READ,
		(PF) icpHandleRead,
		(caddr_t) rw_state_machine);
	    return COMM_OK;
	default:
	    /* Len == 0 means connection closed; otherwise,  would not have been
	     * called by comm_select(). */
	    debug(12, 1, "icpHandleRead: FD %d: read failure: %s\n",
		fd, len == 0 ? "connection closed" : xstrerror());
	    rw_state_machine->handler(fd,
		rw_state_machine->buf,
		rw_state_machine->offset,
		COMM_ERROR,
		rw_state_machine->client_data);
	    safe_free(rw_state_machine);
	    return COMM_ERROR;
	}
    }
    rw_state_machine->offset += len;

    /* Check for \r\n delimiting end of ascii transmission, or */
    /* if we've read content-length bytes already */
    if ((rw_state_machine->offset >= rw_state_machine->size)
	|| (strstr(rw_state_machine->buf, "\r\n") != (char *) NULL)) {
	rw_state_machine->handler(fd,
	    rw_state_machine->buf,
	    rw_state_machine->offset,
	    COMM_OK,
	    rw_state_machine->client_data);
	safe_free(rw_state_machine);
    } else {
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) icpHandleRead,
	    (caddr_t) rw_state_machine);
    }

    return COMM_OK;
}

/* Select for reading on FD, until SIZE bytes are received.  Call
 * HANDLER when complete. */
void icpRead(fd, bin_mode, buf, size, timeout, handler, client_data)
     int fd;
     int bin_mode;
     char *buf;
     int size;
     int timeout;
     void (*handler) _PARAMS((int fd, char *buf, int size, int errflag, caddr_t data));
     caddr_t client_data;
{
    icpReadWriteData *data = NULL;
    data = (icpReadWriteData *) xcalloc(1, sizeof(icpReadWriteData));
    data->fd = fd;
    data->buf = buf;
    data->size = size;
    data->offset = 0;
    data->handler = handler;
    data->timeout = timeout;
    data->time = cached_curtime;
    data->client_data = client_data;
    comm_set_select_handler(fd,
	COMM_SELECT_READ,
	(PF) icpHandleRead,
	(caddr_t) data);
}

/* Write to FD. */
void icpHandleWrite(fd, rwsm)
     int fd;
     icpReadWriteData *rwsm;
{
    int len = 0;
    int nleft;

    debug(12, 5, "icpHandleWrite: FD %d: off %d: sz %d.\n",
	fd, rwsm->offset, rwsm->size);

    nleft = rwsm->size - rwsm->offset;
    len = write(fd, rwsm->buf + rwsm->offset, nleft);

    if (len == 0) {
	/* We're done */
	if (nleft != 0)
	    debug(12, 2, "icpHandleWrite: FD %d: write failure: connection closed with %d bytes remaining.\n", fd, nleft);
	rwsm->handler(fd,
	    rwsm->buf,
	    rwsm->offset,
	    nleft == 0 ? COMM_OK : COMM_ERROR,
	    rwsm->client_data);
	safe_free(rwsm);
	return;
    }
    if (len < 0) {
	/* An error */
	if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    /* XXX: Re-install the handler rather than giving up. I hope
	     * this doesn't freeze this socket due to some random OS bug
	     * returning EWOULDBLOCK indefinitely.  Ought to maintain a
	     * retry count in rwsm? */
	    debug(12, 10, "icpHandleWrite: FD %d: write failure: %s.\n",
		fd, xstrerror());
	    comm_set_select_handler(fd,
		COMM_SELECT_WRITE,
		(PF) icpHandleWrite,
		(caddr_t) rwsm);
	    return;
	}
	debug(12, 2, "icpHandleWrite: FD %d: write failure: %s.\n",
	    fd, xstrerror());
	rwsm->handler(fd,
	    rwsm->buf,
	    rwsm->offset,
	    COMM_ERROR,
	    rwsm->client_data);
	safe_free(rwsm);
	return;
    }
    /* A successful write, continue */
    rwsm->offset += len;
    if (rwsm->offset < rwsm->size) {
	/* Reinstall the read handler and get some more */
	comm_set_select_handler(fd,
	    COMM_SELECT_WRITE,
	    (PF) icpHandleWrite,
	    (caddr_t) rwsm);
	return;
    }
    rwsm->handler(fd,
	rwsm->buf,
	rwsm->offset,
	COMM_OK,
	rwsm->client_data);
    safe_free(rwsm);
}



/* Select for Writing on FD, until SIZE bytes are sent.  Call
 * HANDLER when complete. */
char *icpWrite(fd, buf, size, timeout, handler, client_data)
     int fd;
     char *buf;
     int size;
     int timeout;
     void (*handler) _PARAMS((int fd, char *buf, int size, int errflag, caddr_t data));
     caddr_t client_data;
{
    icpReadWriteData *data = NULL;

    debug(12, 5, "icpWrite: FD %d: sz %d: tout %d: hndl %p: data %p.\n",
	fd, size, timeout, handler, client_data);

    data = (icpReadWriteData *) xcalloc(1, sizeof(icpReadWriteData));
    data->fd = fd;
    data->buf = buf;
    data->size = size;
    data->offset = 0;
    data->handler = handler;
    data->timeout = timeout;
    data->time = cached_curtime;
    data->client_data = client_data;
    comm_set_select_handler(fd,
	COMM_SELECT_WRITE,
	(PF) icpHandleWrite,
	(caddr_t) data);
    return ((char *) data);
}

void icpSendERRORComplete(fd, buf, size, errflag, state)
     int fd;
     char *buf;
     int size;
     int errflag;
     icpStateData *state;
{
    StoreEntry *entry = NULL;
    debug(12, 4, "icpSendERRORComplete: FD %d: sz %d: err %d.\n",
	fd, size, errflag);

    /* Clean up client side statemachine */
    entry = state->entry;
    debug(12, 1, "icpSendERRORComplete: entry=%p\n", entry);
    icpFreeBufOrPage(state);
    icpCloseAndFree(fd, state, __LINE__);

    /* If storeAbort() has been called, then we don't execute this.
     * If we timed out on the client side, then we need to
     * unregister/unlock */
    if (entry) {
	storeUnregister(entry, fd);
	storeUnlockObject(entry);
    }
}

/* Send ERROR message. */
int icpSendERROR(fd, errorCode, msg, state)
     int fd;
     int errorCode;
     char *msg;
     icpStateData *state;
{
    char *buf = NULL;
    int buf_len = 0;
    int port = 0;

    port = comm_port(fd);
    debug(12, 4, "icpSendERROR: code %d: port %d: msg: '%s'\n",
	errorCode, port, msg);

    debug(12, 1, "icpSendERROR: state=%p  state->entry=%p\n", state, state->entry);

    if (port == COMM_ERROR) {
	/* This file descriptor isn't bound to a socket anymore.
	 * It probably timed out. */
	debug(12, 2, "icpSendERROR: COMM_ERROR msg: %1.80s\n", msg);
	/* Force direct call to free up data structures: */
	state->ptr_to_4k_page = state->buf = NULL;
	icpSendERRORComplete(fd,
	    (char *) NULL,
	    NULL,
	    COMM_ERROR,
	    (caddr_t) state);
	return COMM_ERROR;
    } else if (port == getAsciiPortNum()) {
	/* Error message for the ascii port */
	buf_len = strlen(msg) + 1;	/* XXX: buf_len includes \0? */
	buf = state->ptr_to_4k_page = get_free_4k_page();
	state->buf = NULL;
	memset(buf, '\0', buf_len);
	strcpy(buf, msg);
    } else {
	fatal_dump("This should not happen!\n");
    }
    icpWrite(fd, buf, buf_len, 30, icpSendERRORComplete, (caddr_t) state);
    return COMM_OK;
}

/* Send available data from an object in the cache.  This is called either
 * on select for  write or directly by icpHandleStore. */

int icpSendMoreData(fd, state)
     int fd;
     icpStateData *state;
{
    char *buf = NULL;
    char *p = NULL;
    StoreEntry *entry = state->entry;
    int result = COMM_ERROR, max_len = 0;
    icp_common_t *header = &state->header;
    int buf_len, len;

    debug(12, 5, "icpSendMoreData: <URL:%s> sz %d: len %d: off %d.\n",
	entry->url, entry->object_len,
	has_mem_obj(entry) ? entry->mem_obj->e_current_len : 0, state->offset);

    p = state->ptr_to_4k_page = buf = get_free_4k_page();
    state->buf = NULL;

    /* Set maxlen to largest amount of data w/o header
     * place p pointing to beginning of data portion of message */

    buf_len = 0;		/* No header for ascii mode */

    max_len = ICP_SENDMOREDATA_BUF - buf_len;
    /* Should limit max_len to something like 1.5x last successful write */
    p += buf_len;

    storeClientCopy(state->entry, state->offset, max_len, p, &len, fd);

    buf_len += len;

    if ((state->offset == 0) && (header->opcode != ICP_OP_DATABEG)) {
	header->opcode = ICP_OP_DATABEG;
    } else if ((entry->mem_obj->e_current_len == entry->object_len) &&
	    ((entry->object_len - state->offset) == len) &&
	(entry->status != STORE_PENDING)) {
	/* No more data; this is the last message. */
	header->opcode = ICP_OP_DATAEND;
    } else {
	/* We know there is more data to come. */
	header->opcode = ICP_OP_DATA;
    }
    debug(12, 6, "icpSendMoreData: opcode %d: len %d.\n",
	header->opcode, entry->object_len);

    header->length = buf_len;

    state->offset += len;

    /* Do this here, so HandleStoreComplete can tell whether more data 
     * needs to be sent. */
    icpWrite(fd, buf, buf_len, 30, icpHandleStoreComplete, (caddr_t) state);
    result = COMM_OK;
    return result;
}

/* Called by storage manager when more data arrives from source. 
 * Starts state machine towards client with new batch of data or
 * error messages.  We get here by invoking the handlers in the
 * pending list.
 */
static void icpHandleStore(fd, entry, state)
     int fd;
     StoreEntry *entry;
     icpStateData *state;
{
    debug(12, 5, "icpHandleStore: FD %d: off %d: <URL:%s>\n",
	fd, state->offset, entry->url);

    debug(12, 5, "icpHandleStore: entry=%p, state=%p\n", entry, state);
    if (entry->status == STORE_ABORTED) {
#ifdef CAN_WE_GET_AWAY_WITHOUT_THIS
	storeUnlockObject(entry);
	state->entry = NULL;	/* Don't use a subsequently freed storeEntry */
#endif
	state->log_type = entry->mem_obj->abort_code;
	debug(12, 1, "icpHandleStore: abort_code=%d\n", entry->mem_obj->abort_code);
	state->ptr_to_4k_page = NULL;	/* Nothing to deallocate */
	state->buf = NULL;	/* Nothing to deallocate */
	icpSendERROR(fd,
	    ICP_ERROR_TIMEDOUT,
	    entry->mem_obj->e_abort_msg,
	    state);
	return;
    }
    state->entry = entry;
    icpSendMoreData(fd, state);
}

void icpHandleStoreComplete(fd, buf, size, errflag, state)
     int fd;
     char *buf;
     int size;
     int errflag;
     icpStateData *state;
{
    debug(12, 5, "icpHandleStoreComplete: FD %d: sz %d: err %d: off %d: len %d: tsmp %d: lref %d.\n",
	fd, size, errflag,
	state->offset, state->entry->object_len,
	state->entry->timestamp, state->entry->lastref);

    icpFreeBufOrPage(state);
    if (errflag) {
	/* if runs in quick abort mode, set flag to tell 
	 * fetching module to abort the fetching */
	CheckQuickAbort(state);
	/* Log the number of bytes that we managed to read */
	CacheInfo->proto_touchobject(CacheInfo,
	    proto_url_to_id(state->entry->url),
	    state->offset);
	/* Now we release the entry and DON'T touch it from here on out */
	storeUnregister(state->entry, fd);
	storeUnlockObject(state->entry);
	icpCloseAndFree(fd, state, __LINE__);
    } else if (state->offset < state->entry->mem_obj->e_current_len) {
	/* More data available locally; write it now */
	icpSendMoreData(fd, state);
    } else
	/* We're finished case */
	if (state->offset == state->entry->object_len &&
	state->entry->status != STORE_PENDING) {
	storeUnregister(state->entry, fd);
	CacheInfo->proto_touchobject(CacheInfo,
	    CacheInfo->proto_id(state->entry->url),
	    state->offset);
	storeUnlockObject(state->entry);
	icpCloseAndFree(fd, state, __LINE__);
    } else {
	/* More data will be coming from primary server; register with 
	 * storage manager. */
	storeRegister(state->entry, fd, (PIF) icpHandleStore, (caddr_t) state);
    }
}

int icpDoQuery(fd, state)
     int fd;
     icpStateData *state;
{
    state->buf = state->ptr_to_4k_page = NULL;	/* Nothing to free */
    /* XXX not implemented over tcp. */
    icpSendERROR(fd, ICP_ERROR_INTERNAL, "not implemented over tcp", state);
    return COMM_OK;
}

/*
 * Below, we check whether the object is a hit or a miss.  If it's a hit,
 * we check whether the object is still valid or whether it is a MISS_TTL.
 */
void icp_hit_or_miss(fd, usm)
     int fd;
     icpStateData *usm;
{
    char *url = usm->url;
    char *key = NULL;
    char *mime_hdr = usm->mime_hdr;
    StoreEntry *entry = NULL;
    int lock = 0;

    debug(12, 4, "icp_hit_or_miss: %s <URL:%s>\n", usm->type, url);

    if (usm->type_id == REQUEST_OP_GET) {
	key = url;
    } else {
	key = storeGenerateKey(usm->url, usm->type_id);
    }

    entry = storeGet(key);

    if (entry) {
	if (storeEntryValidToSend(entry) &&
	    (mime_hdr && !mime_refresh_request(mime_hdr)) &&
	    ((lock = storeLockObject(entry)) == 0)) {
	    debug(12, 4, "icp_hit_or_miss: sending from store.\n");

	    /* We HOLD a lock on object "entry" */
	    tmp_in_addr.s_addr = htonl(usm->header.shostid);
	    usm->log_type = LOG_TCP_HIT;
	    CacheInfo->proto_hit(CacheInfo, CacheInfo->proto_id(entry->url));

	    /* Reset header for reply. */
	    memset(&usm->header, 0, sizeof(icp_common_t));
	    usm->header.version = ICP_VERSION_CURRENT;
	    usm->header.reqnum = 0;
	    usm->header.shostid = 0;
	    usm->entry = entry;
	    usm->offset = 0;

	    /* Send object to requestor */
	    entry->refcount++;	/* HIT CASE */

	    /* Handle IMS */
/*          if(mime_get_header(mime_hdr,"if-modified-since"))
 * icpSendIMS(fd, usm);
 * else
 */ icpSendMoreData(fd, usm);
	    return;
	}
	/* We do NOT hold a lock on the existing "entry" because we're
	 * about to eject it */
	if (lock < 0)
	    debug(12, 0, "icp_hit_or_miss: swap file open failed\n");
	tmp_in_addr.s_addr = htonl(usm->header.shostid);
	usm->log_type = LOG_TCP_EXPIRED;
	CacheInfo->proto_miss(CacheInfo, CacheInfo->proto_id(url));
	icpProcessMISS(fd, usm);
	return;

    }
    /* This object isn't in the cache.  We do not hold a lock yet */
    tmp_in_addr.s_addr = htonl(usm->header.shostid);
    usm->log_type = LOG_TCP_MISS;
    CacheInfo->proto_miss(CacheInfo, CacheInfo->proto_id(url));
    icpProcessMISS(fd, usm);
    return;
}

/*
 * Prepare to fetch the object as it's a cache miss of some kind.
 * The calling client should NOT hold a lock on object at this
 * time, as we're about to release any TCP_MISS version of the object.
 */
static int icpProcessMISS(fd, usm)	/* Formally icpProcessSENDA */
     int fd;
     icpStateData *usm;
{
    char *url = usm->url;
    char *key = NULL;
    char *type = usm->type;
    char *mime_hdr = usm->mime_hdr;
    StoreEntry *entry = NULL;

    debug(12, 4, "icpProcessMISS: %s <URL:%s>\n", type, url);
    debug(12, 10, "icpProcessMISS: mime_hdr: '%s'\n", mime_hdr);

    if (usm->type_id == REQUEST_OP_GET) {
	key = url;
    } else {
	key = storeGenerateKey(usm->url, usm->type_id);
    }

    entry = storeGet(key);

    if (entry) {
	/* get rid of the old entry */
	if (storeEntryLocked(entry)) {
	    /* change original hash key to get out of the new object's way */
	    if (!storeOriginalKey(entry))
		fatal_dump("ProcessMISS: Object located by changed key?\n");
	    storeChangeKey(entry);
	    debug(12, 4, "icpProcessMISS: Key Change: '%s' <URL:%s>\n",
		entry->key, entry->url);
	    BIT_SET(entry->flag, RELEASE_REQUEST);
	} else {
	    storeRelease(entry);
	}
    }
    entry = storeAdd(url,
	type,
	mime_hdr,
	proto_cachable(url, type, mime_hdr),
	usm->html_request,
	usm->type_id);

    entry->refcount++;		/* MISS CASE */
    entry->mem_obj->fd_of_first_client = fd;
    fd_table[fd].store_entry = entry;
    BIT_SET(entry->flag, IP_LOOKUP_PENDING);
    storeLockObject(entry);

    /*Reset header fields for  reply. */
    memset(&usm->header, 0, sizeof(icp_common_t));
    usm->header.version = ICP_VERSION_CURRENT;
    usm->header.reqnum = 0;
    usm->header.shostid = 0;
    usm->entry = entry;
    usm->offset = 0;

    /* Register with storage manager to receive updates when data comes in. */
    storeRegister(entry, fd, (PIF) icpHandleStore, (caddr_t) usm);

    return (protoDispatch(fd, url, usm->entry));
}

void icpProcessUrl(fd, buf, size, flag, usm)
     int fd;
     char *buf;
     int size;
     int flag;
     icpStateData *usm;
{
    if (flag || size < usm->header.length - sizeof(icp_common_t)) {
	debug(12, 1, "icpProcessUrl: failure trying to read host id.\n");
	safe_free(buf);
	usm->buf = usm->ptr_to_4k_page = NULL;	/* Nothing to free */
	icpSendERROR(fd, ICP_ERROR_INTERNAL, "error reading host id", usm);
    } else {
	/* Extract hostid. */
	memcpy(&usm->query_host, buf, sizeof(u_num32));
	usm->url = (char *) xstrdup(buf + sizeof(u_num32));
	usm->type = xstrdup("GET");
	usm->mime_hdr = NULL;

	safe_free(buf);

	/* Process request. */
	if (usm->header.opcode == ICP_OP_SEND) {
	    debug(12, 5, "icpProcessUrl: processing ICP_OP_SEND\n");
	    icp_hit_or_miss(fd, usm);
	} else if (usm->header.opcode == ICP_OP_SENDA) {
	    debug(12, 5, "icpProcessUrl: processing ICP_OP_SENDA\n");
	    icpProcessMISS(fd, usm);
	} else if (usm->header.opcode == ICP_OP_QUERY) {
	    debug(12, 5, "icpProcessUrl: processing ICP_OP_QUERY\n");
	    icpDoQuery(fd, usm);
	} else {
	    debug(12, 1, "icpProcessUrl: Invalid OPCODE: %d.\n", usm->header.opcode);
	}
    }
}

int icpProcessHeader(fd, buf_notused, size, flag, state)
     int fd;
     char *buf_notused;
     int size;
     int flag;
     icpStateData *state;
{
    int result = COMM_ERROR;

    debug(12, 4, "icpProcessHeader: FD %d.\n", fd);

    if (flag || size < sizeof(icp_common_t)) {
	debug(12, 1, "icpProcessHeader: FD %d: header read failure.\n", fd);
	state->buf = state->ptr_to_4k_page = NULL;	/* Nothing to free */
	icpSendERROR(fd, ICP_ERROR_INTERNAL, "error reading header", state);
	result = COMM_ERROR;
    } else {
	short op = ntohs(state->header.opcode);
	if (op == ICP_OP_SEND || op == ICP_OP_SENDA || op == ICP_OP_QUERY) {
	    /* Read query host id & url. */
	    int buf_size;
	    char *buf;

	    icp_common_t *hp = &state->header;
	    hp->opcode = op;
	    /* XXX Do these macros work ok in this fashion? */
	    hp->version = hp->version;
	    hp->length = ntohs(hp->length);
	    hp->reqnum = ntohl(hp->reqnum);
	    hp->shostid = ntohl(hp->shostid);

	    /* Allocate buffer for  hostid and url. */
	    buf_size = hp->length - sizeof(icp_common_t);
	    buf = xcalloc(buf_size, sizeof(char));

	    /* Schedule read of host id and url. */
	    (void) icpRead(fd, TRUE, buf, buf_size, 30, icpProcessUrl, (caddr_t) state);
	} else {
	    debug(12, 1, "icpProcessHeader: FD %d: invalid OPCODE: %d\n", fd, op);
	    state->buf = state->ptr_to_4k_page = NULL;	/* Nothing to free */
	    icpSendERROR(fd, ICP_ERROR_INTERNAL, "invalid opcode", state);
	    result = COMM_ERROR;
	}
    }
    return result;
}


int icpUdpReply(fd, queue)
     int fd;
     icpUdpData *queue;
{
    int result = COMM_OK;
    icpUdpData *dp = NULL;

    dp = UdpQueue;
    /* Disable handler, in case of errors. */
    comm_set_select_handler(fd, COMM_SELECT_WRITE, 0, 0);

    if (comm_udp_sendto(fd, &queue->address, sizeof(struct sockaddr_in),
	    queue->msg, queue->len) < 0) {
	debug(12, 1, "icpUdpReply: error sending\n");
	result = COMM_ERROR;
    }
    /* Don't close socket, as we need it for new incoming requests.  Just remove
     * handler for writes. */
    UdpQueue = UdpQueue->next;
    if (!UdpQueue)
	comm_set_select_handler(fd,
	    COMM_SELECT_WRITE,
	    0,
	    0);
    else
	comm_set_select_handler(fd,
	    COMM_SELECT_WRITE,
	    (PF) icpUdpReply,
	    (caddr_t) UdpQueue);
    safe_free(dp->msg);
    safe_free(dp);

    return result;
}

int icpUdpMiss(fd, url, reqheaderp, from)
     int fd;
     char *url;
     icp_common_t *reqheaderp;
     struct sockaddr_in *from;
{
    char *buf = NULL;
    int buf_len = sizeof(icp_common_t) + strlen(url) + 1;
    icp_common_t *headerp = NULL;
    icpUdpData *data = (icpUdpData *) xmalloc(sizeof(icpUdpData));
    struct sockaddr_in our_socket_name;
    int sock_name_length = sizeof(our_socket_name);

    if (getsockname(fd, (struct sockaddr *) &our_socket_name,
	    &sock_name_length) == -1) {
	debug(12, 1, "icpUdpMiss: FD %d: getsockname failure: %s\n",
	    fd, xstrerror());
    }
    debug(12, 5, "icpUdpMiss: FD %d: %s: <URL:%s>\n", fd,
	inet_ntoa(our_socket_name.sin_addr), url);

    memset(data, '\0', sizeof(icpUdpData));
    memcpy(&data->address, from, sizeof(struct sockaddr_in));

    buf = xcalloc(buf_len, 1);
    headerp = (icp_common_t *) buf;
    headerp->opcode = ICP_OP_MISS;
    headerp->version = ICP_VERSION_CURRENT;
    headerp->length = htons(buf_len);
    headerp->reqnum = htonl(reqheaderp->reqnum);
/*  memcpy(headerp->auth, , ICP_AUTH_SIZE); */
    headerp->shostid = htonl(our_socket_name.sin_addr.s_addr);

    memcpy(buf + sizeof(icp_common_t), url, strlen(url) + 1);
    data->msg = buf;
    data->len = buf_len;
    UdpQueue = AppendUdp(data, UdpQueue);

    comm_set_select_handler(fd, COMM_SELECT_WRITE, (PF) icpUdpReply,
	(caddr_t) UdpQueue);

    return COMM_OK;
}

int icpUdpSend(fd, url, reqheaderp, to, opcode)
     int fd;
     char *url;
     icp_common_t *reqheaderp;
     struct sockaddr_in *to;
     icp_opcode opcode;
{
    char *buf = NULL;
    int buf_len = sizeof(icp_common_t) + strlen(url) + 1;
    icp_common_t *headerp = NULL;
    icpUdpData *data = (icpUdpData *) xmalloc(sizeof(icpUdpData));
    struct sockaddr_in our_socket_name;
    int sock_name_length = sizeof(our_socket_name);
    char *urloffset = NULL;

    if (getsockname(fd, (struct sockaddr *) &our_socket_name,
	    &sock_name_length) == -1) {
	debug(12, 1, "icpUdpSend: FD %d: getsockname failure: %s\n",
	    fd, xstrerror());
    }
    memset(data, '\0', sizeof(icpUdpData));
    memcpy(&data->address, to, sizeof(struct sockaddr_in));

    if (opcode == ICP_OP_QUERY)
	buf_len += sizeof(u_num32);
    buf = xcalloc(buf_len, 1);

    headerp = (icp_common_t *) buf;
    headerp->opcode = opcode;
    headerp->version = ICP_VERSION_CURRENT;
    headerp->length = htons(buf_len);
    headerp->reqnum = htonl(reqheaderp->reqnum);
/*  memcpy(headerp->auth, , ICP_AUTH_SIZE); */
    headerp->shostid = htonl(our_socket_name.sin_addr.s_addr);

    urloffset = buf + sizeof(icp_common_t);

    if (opcode == ICP_OP_QUERY)
	urloffset += sizeof(u_num32);
    /* it's already zero filled by xcalloc */
    memcpy(urloffset, url, strlen(url));
    data->msg = buf;
    data->len = buf_len;

    UdpQueue = AppendUdp(data, UdpQueue);

    debug(12, 4, "icpUdpSend: op %d: to %s: sz %d: <URL:%s>\n", opcode,
	inet_ntoa(to->sin_addr), buf_len, url);

    comm_set_select_handler(fd,
	COMM_SELECT_WRITE,
	(PF) icpUdpReply,
	(caddr_t) UdpQueue);

    return COMM_OK;
}

int icpHandleUdp(sock, not_used)
     int sock;
     caddr_t not_used;
{

    int result = 0;
    struct sockaddr_in from;
    int from_len;
    static char buf[ICP_MAX_UDP_SIZE];
    int len;
    icp_common_t header;
    icp_common_t *headerp = NULL;
    StoreEntry *entry = NULL;
    char *url = NULL;

    from_len = sizeof(from);
    memset(&from, 0, from_len);
    /* zero filled to make sure url is terminated. */
    memset(buf, 0, ICP_MAX_UDP_SIZE);


    if ((len = comm_udp_recv(sock, buf, ICP_MAX_UDP_SIZE - 1, &from, &from_len)) < 0) {
	debug(12, 1, "icpHandleUdp: FD %d: error receiving.\n", sock);
	/* don't exit here. soft error. */
	/* exit(1); */
	comm_set_select_handler(sock, COMM_SELECT_READ, icpHandleUdp, 0);
	return result;
    }
    debug(12, 4, "icpHandleUdp: FD %d: received %d bytes from %s.\n", sock, len,
	inet_ntoa(from.sin_addr));

    if (len < sizeof(icp_common_t)) {
	debug(12, 4, "icpHandleUdp: Bad UDP packet, Ignored. Size: %d < Header: %d\n",
	    len, sizeof(icp_common_t));
    } else {

	/* Get fields from incoming message. */
	headerp = (icp_common_t *) buf;
	header.opcode = headerp->opcode;
	header.length = ntohs(headerp->length);
	header.reqnum = ntohs(headerp->reqnum);
	header.shostid = ntohs(headerp->shostid);
	header.version = ntohs(headerp->version);
	/*  memcpy(headerp->auth, , ICP_AUTH_SIZE); */

	switch (header.opcode) {
	case ICP_OP_QUERY:
	    if (len < sizeof(icp_common_t)) {
		/* at least it has to have \0 as a URL */
		debug(12, 4, "icpHandleUdp: Bad ICP_OP_QUERY packet, Ignored.\n");
		debug(12, 4, "Size: %d < Min: %d\n", len,
		    sizeof(icp_common_t) + sizeof(u_num32) + 1);
		break;
	    }
	    /* We have a valid packet */
	    url = buf + sizeof(header) + sizeof(u_num32);
	    if (ip_access_check(from.sin_addr, proxy_ip_acl) == IP_DENY) {
		debug(12, 2, "icpHandleUdp: Access Denied for %s.\n",
		    inet_ntoa(from.sin_addr));
		CacheInfo->log_append(CacheInfo,	/* UDP_DENIED */
		    url,
		    inet_ntoa(from.sin_addr),
		    0,
		    log_tags[LOG_UDP_DENIED],
		    "ICP_OP_QUERY");
		break;
	    }
	    /* The peer is allowed to use this cache */
	    entry = storeGet(url);
	    debug(12, 5, "icpHandleUdp: OPCODE: ICP_OP_QUERY\n");
	    if (entry &&
		(entry->status == STORE_OK) &&
		((entry->expires - UDP_HIT_THRESH) > cached_curtime)) {
		/* Send "HIT" message. */
		/* STAT */
		CacheInfo->log_append(CacheInfo,	/* UDP_HIT */
		    entry->url,
		    0,		/* inet_ntoa(from.sin_addr), */
		    entry->object_len,
		    log_tags[LOG_UDP_HIT],
		    "ICP_OP_QUERY");
		CacheInfo->proto_hit(CacheInfo,
		    CacheInfo->proto_id(entry->url));
		icpUdpSend(sock, url, &header, &from, ICP_OP_HIT);
		break;
	    }
	    /* Send "MISS" message. */
	    /* STAT */
	    CacheInfo->log_append(CacheInfo,	/* UDP_MISS */
		url,
		inet_ntoa(from.sin_addr),
		0,
		log_tags[LOG_UDP_MISS],
		"ICP_OP_QUERY");
	    CacheInfo->proto_miss(CacheInfo,
		CacheInfo->proto_id(url));

	    icpUdpMiss(sock, url, &header, &from);
	    break;

	case ICP_OP_HIT:
	    url = buf + sizeof(header);
	    debug(12, 4, "icpHandleUdp: HIT from %s for <URL:%s>.\n",
		inet_ntoa(from.sin_addr), url);
	    if ((entry = storeGet(url)) == NULL) {
		debug(12, 4, "icpHandleUdp: Ignoring UDP HIT for NULL Entry.\n");
		break;
	    }
	    neighborsUdpAck(sock, url, &header, &from, entry);
	    break;

	case ICP_OP_SECHO:
	    url = buf + sizeof(header);
	    debug(12, 4, "icpHandleUdp: SECHO from %s for <URL:%s>\n",
		inet_ntoa(from.sin_addr), url);
	    if ((entry = storeGet(url)) == NULL) {
		debug(12, 4, "icpHandleUdp: Ignoring UDP SECHO for NULL Entry.\n");
		break;
	    }
	    neighborsUdpAck(sock, url, &header, &from, entry);
	    break;

	case ICP_OP_DECHO:
	    url = buf + sizeof(header);
	    debug(12, 4, "icpHandleUdp: DECHO from %s for <URL:%s>\n",
		inet_ntoa(from.sin_addr), url);
	    if ((entry = storeGet(url)) == NULL) {
		debug(12, 4, "icpHandleUdp: Ignoring UDP DECHO for NULL Entry.\n");
		break;
	    }
	    neighborsUdpAck(sock, url, &header, &from, entry);
	    break;

	case ICP_OP_MISS:
	    url = buf + sizeof(header);
	    debug(12, 4, "icpHandleUdp: MISS from %s for <URL:%s>\n",
		inet_ntoa(from.sin_addr), url);
	    if ((entry = storeGet(url)) == NULL) {
		debug(12, 4, "icpHandleUdp: Ignoring UDP MISS for NULL Entry.\n");
		break;
	    }
	    neighborsUdpAck(sock, url, &header, &from, entry);
	    break;

	default:
	    debug(12, 4, "icpHandleUdp: OPCODE: %d\n", header.opcode);
	    break;
	}
    }
    comm_set_select_handler(sock,
	COMM_SELECT_READ,
	icpHandleUdp,
	0);
    return result;
}

static char *do_append_domain(url, ad)
     char *url;
     char *ad;
{
    char *b = NULL;		/* beginning of hostname */
    char *e = NULL;		/* end of hostname */
    char *p = NULL;
    char *u = NULL;
    int lo;
    int ln;
    int adlen;

    if (!(b = strstr(url, "://")))	/* find beginning of host part */
	return NULL;
    b += 3;
    if (!(e = strchr(b, '/')))	/* find end of host part */
	e = b + strlen(b);
    if ((p = strchr(b, '@')) && p < e)	/* After username info */
	b = p + 1;
    if ((p = strchr(b, ':')) && p < e)	/* Before port */
	e = p;
    if ((p = strchr(b, '.')) && p < e)	/* abort if host has dot already */
	return NULL;
    lo = strlen(url);
    ln = lo + (adlen = strlen(ad));
    u = xcalloc(ln + 1, 1);
    strncpy(u, url, (e - url));	/* copy first part */
    b = u + (e - url);
    p = b + adlen;
    strncpy(b, ad, adlen);	/* copy middle part */
    strncpy(p, e, lo - (e - url));	/* copy last part */
    return (u);
}


/*
 *  parseAsciiUrl()
 * 
 *  Called by
 *    asciiProcessInput() after the request has been read
 *  Calls
 *    mime_process()
 *    do_append_domain()
 *  Returns
 *    0 on error
 *    1 on success
 */
int parseAsciiUrl(input, astm)
     char *input;
     icpStateData *astm;
{
    char *cmd = NULL;
    char *url = NULL;
    char *token = NULL;
    char *t = NULL;
    char *mime_end = NULL;
    char *xbuf = NULL;
    int free_url = 0;
    int content_length = 0;
    int xtra_bytes = 0;
    char *ad = NULL;

    /* do a non-destructive test first */
    if (strstr(input, "HTTP/1.0\r\n")) {
	/* HTTP/1.0 Request */
	mime_end = strstr(input, "\r\n\r\n");
	if (mime_end == NULL) {
	    debug(12, 3, "parseAsciiUrl: Got partial HTTP request.\n");
	    return 0;		/* request is not complete yet. */
	}
	mime_end += 4;

	/* Check content-length for POST requests */
	xbuf = xstrdup(input);
	for (t = strtok(xbuf, crlf); t; t = strtok(NULL, crlf)) {
	    if (strncasecmp(t, "Content-Length:", 15) == 0) {
		content_length = atoi(t + 16);
		break;
	    }
	}
	xfree(xbuf);
	if (content_length > 0) {
	    xtra_bytes = strlen(mime_end);
	    debug(12, 3, "parseAsciiUrl: Content-Length=%d\n", content_length);
	    debug(12, 3, "parseAsciiUrl: xtra_bytes=%d\n", xtra_bytes);
	    if (xtra_bytes < content_length) {
		astm->bytes_needed = content_length - xtra_bytes;
		return 0;	/* request is not complete yet. */
	    } else {
		astm->bytes_needed = 0;
	    }
	}
    }
    cmd = (char *) strtok(input, "\t ");
    if (cmd == NULL || isascii(cmd[0]) == 0) {
	debug(12, 5, "parseAsciiUrl: Failed for '%s'\n", input);
	return 0;
    }
    if (strcasecmp(cmd, "GET") == 0) {
	if (strcmp(cmd, "GET") == 0)
	    astm->html_request = 1;
	else
	    astm->html_request = 0;
	url = (char *) strtok(NULL, "\n\r\t ");
	astm->type = xstrdup("GET");
	astm->type_id = REQUEST_OP_GET;
	if ((token = (char *) strtok(NULL, "")))
	    astm->mime_hdr = xstrdup(token);
	else
	    astm->mime_hdr = xstrdup(" ");
    } else if (strcasecmp(cmd, "POST") == 0) {
	astm->html_request = 1;
	url = (char *) strtok(NULL, "\n\r\t ");
	astm->type = xstrdup((char *) "POST");
	astm->type_id = REQUEST_OP_POST;
	if ((token = (char *) strtok(NULL, "")))
	    astm->mime_hdr = xstrdup(token);
	else
	    astm->mime_hdr = xstrdup(" ");
    } else if (strcasecmp(cmd, "HEAD") == 0) {
	astm->html_request = 1;
	url = (char *) strtok(NULL, "\n\r\t ");
	astm->type = xstrdup((char *) "HEAD");
	astm->type_id = REQUEST_OP_HEAD;
	if ((token = (char *) strtok(NULL, "")))
	    astm->mime_hdr = xstrdup(token);
	else
	    astm->mime_hdr = xstrdup(" ");
    } else {
	astm->type = xstrdup("GET");
	astm->type_id = REQUEST_OP_GET;
	astm->html_request = 0;
	astm->mime_hdr = xstrdup(" ");
    }

    if (url == NULL) {
	debug(12, 5, "parseAsciiUrl: NULL URL: %s\n", input);
	return 0;
    }
    if ((ad = getAppendDomain())) {
	if ((t = do_append_domain(url, ad))) {
	    url = t;
	    free_url = 1;
	    /* NOTE: We don't have to free the old url pointer
	     * because it points to inside xbuf. But
	     * do_append_domain() allocates memory so set a flag
	     * if the url should be freed later. */
	}
    }
    if ((t = strchr(url, '\r')))	/* remove CR */
	*t = '\0';
    if ((t = strchr(url, '\n')))	/* remove NL */
	*t = '\0';
    if ((t = strchr(url, '#')))	/* remove HTML anchors */
	*t = '\0';

    /* see if we running in httpd_accel_mode, if so got to convert it to URL */
    if (httpd_accel_mode && url[0] == '/') {
	if (!vhost_mode) {
	    /* prepend the accel prefix */
	    astm->url = xcalloc(strlen(getAccelPrefix()) +
		strlen(url) +
		1, 1);
	    sprintf(astm->url, "%s%s", getAccelPrefix(), url);
	} else {
	    /* Put the local socket IP address as the hostname */
	    astm->url = xcalloc(strlen(url) + 24, 1);
	    sprintf(astm->url, "http://%s%s",
		inet_ntoa(astm->me.sin_addr), url);
	}
	astm->accel_request = 1;
    } else {
	astm->url = xstrdup(url);
	astm->accel_request = 0;
    }
    debug(12, 5, "parseAsciiUrl: %s: <URL:%s>: mime_hdr '%s'.\n",
	astm->type, astm->url, astm->mime_hdr);
    if (free_url)
	safe_free(url);
    return 1;
}

ip_access_type second_ip_acl_check(fd_unused, astm)
     int fd_unused;
     icpStateData *astm;
{
    if (astm->accel_request)
	return ip_access_check(astm->peer.sin_addr, accel_ip_acl);
    return ip_access_check(astm->peer.sin_addr, proxy_ip_acl);
}


/* Also rewrites URLs... */
static int check_valid_url(fd, astm)
     int fd;
     icpStateData *astm;
{
    static char proto[MAX_URL];
    static char host[MAX_URL];
    static char urlpath[MAX_URL];
    char *t = NULL;
    if (sscanf(astm->url, "%[^:]://%[^/]%s", proto, host, urlpath) != 3)
	return 0;
    for (t = host; *t; t++)
	*t = tolower(*t);
    if ((t = strchr(host, ':'))) {
	switch (atoi(t + 1)) {
	case 80:
	    if (!strcmp(proto, "http"))
		*t = '\0';
	    break;
	case 21:
	    if (!strcmp(proto, "ftp"))
		*t = '\0';
	    break;
	case 70:
	    if (!strcmp(proto, "gopher"))
		*t = '\0';
	    break;
	default:
	    break;
	}
    }
    sprintf(astm->url, "%s://%s%s", proto, host, urlpath);
    return 1;
}



#define ASCII_INBUF_SIZE 4096
/*
 * asciiProcessInput()
 * 
 * Handler set by
 *   asciiHandleConn()
 * Called by
 *   comm_select() when data has been read
 * Calls
 *   parseAsciiUrl()
 *   icp_hit_or_miss()
 *   icpSendERROR()
 */
void asciiProcessInput(fd, buf, size, flag, astm)
     int fd;
     char *buf;
     int size;
     int flag;
     icpStateData *astm;
{
    char client_msg[64];
    char *orig_url_ptr = NULL;
    int parser_return_code = 0;
    int k;

    /*
     *        Be careful about buf and astm->url, erhyuan ***
     *        When it was called first time, buf == astm->url,
     *        buf == (astm->url + offset) after second call.
     *        To prevent freeing twice or wrong pointer,
     *        use "safe_free(astm->url) instead of "safe_free(buf)".
     *
     *        parseAsciiUrl will allocate a new memory for astm->url,
     *        use orig_url_ptr to free original allocated memory 
     */

    debug(12, 4, "asciiProcessInput: FD %d: reading request...\n", fd);

    if (flag != COMM_OK) {
	/* connection closed by foreign host */
	icpCloseAndFree(fd, astm, __LINE__);
	return;
    }
    parser_return_code = parseAsciiUrl(orig_url_ptr = astm->url, astm);
    if (parser_return_code == 1) {
	if (check_valid_url(fd, astm) == 0) {
	    debug(12, 5, "Invalid URL: %s\n", astm->url);
	    astm->buf = xstrdup(cached_error_url(astm->url,
		    ERR_INVALID_URL,
		    NULL));
	    astm->ptr_to_4k_page = NULL;
	    icpWrite(fd,
		astm->buf,
		strlen(astm->buf),
		30,
		icpSendERRORComplete,
		(caddr_t) astm);
	    safe_free(orig_url_ptr);
	} else if (blockCheck(astm->url)) {
	    astm->log_type = LOG_TCP_BLOCK;
	    astm->buf = xstrdup(cached_error_url(astm->url, ERR_URL_BLOCKED, NULL));
	    icpWrite(fd,
		astm->buf,
		strlen(astm->buf),
		30,
		icpSendERRORComplete,
		(caddr_t) astm);
	    safe_free(orig_url_ptr);
	} else if (second_ip_acl_check(fd, astm) == IP_DENY) {
	    sprintf(tmp_error_buf,
		"ACCESS DENIED\n\nYour IP address (%s) is not authorized to access cached at %s.\n\n",
		inet_ntoa(astm->peer.sin_addr),
		getMyHostname());
	    astm->buf = xstrdup(tmp_error_buf);
	    astm->ptr_to_4k_page = NULL;
	    icpWrite(fd,
		astm->buf,
		strlen(tmp_error_buf),
		30,
		icpSendERRORComplete,
		(caddr_t) astm);
	    astm->log_type = LOG_TCP_DENIED;
	    safe_free(orig_url_ptr);
	} else {
	    sprintf(client_msg, "%16.16s %-4.4s %-40.40s",
		fd_note(fd, 0),
		astm->type,
		astm->url);
	    fd_note(fd, client_msg);
	    astm->offset = strlen(astm->url);
	    icp_hit_or_miss(fd, astm);
	    safe_free(orig_url_ptr);
	}
    } else if ((parser_return_code == 0) &&
	(astm->offset + size < ASCII_INBUF_SIZE)) {
	/*
	 *    Partial request received; reschedule until parseAsciiUrl()
	 *    is happy with the input
	 */
	astm->offset += size;
	k = ASCII_INBUF_SIZE - 1 - astm->offset;
	if (0 < astm->bytes_needed && astm->bytes_needed < k)
	    k = astm->bytes_needed;
	icpRead(fd,
	    FALSE,
	    astm->url + astm->offset,
	    k,
	    30,
	    asciiProcessInput, (caddr_t) astm);
    } else {
	/* either parser return -1, or the read buffer is full. */
	debug(12, 1, "asciiProcessInput: FD %d: read failure.\n", fd);
	astm->buf = NULL;
	astm->ptr_to_4k_page = NULL;
	icpSendERROR(fd, ICP_ERROR_INTERNAL, "error reading request", astm);
	safe_free(astm->url);
    }
}



/* general lifetime handler for ascii connection */
void asciiConnLifetimeHandle(fd, data)
     int fd;
     caddr_t data;
{
    icpStateData *astm = (icpStateData *) data;
    PF handler;
    caddr_t client_data;
    icpReadWriteData *rw_state = NULL;

    debug(12, 2, "asciiConnLifetimeHandle: Socket: %d lifetime is expired. Free up data structure.\n", fd);

    /* If a write handler was installed, we were in the middle of an
     * icpWrite and we're going to need to deallocate the icpReadWrite
     * buffer.  These come from icpSendMoreData and from icpSendERROR, both
     * of which allocate 4k buffers. */

    handler = NULL;
    client_data = NULL;
    comm_get_select_handler(fd,
	COMM_SELECT_WRITE,
	(PF *) & handler,
	(caddr_t *) & client_data);
    if ((handler != NULL) && (client_data != NULL)) {
	rw_state = (icpReadWriteData *) client_data;
	if (rw_state->buf)
	    put_free_4k_page(rw_state->buf);
	safe_free(rw_state);
    }
    if (astm->entry) {
	CheckQuickAbort(astm);
	storeUnregister(astm->entry, fd);

	/* We are now detached from the store entry but not the
	 * DNS handler for it */
	if (astm->url) {
	    /* Unregister us from the dnsserver pending list and cause a DNS
	     * related storeAbort() for other attached clients.  If this
	     * doesn't succeed, then the fetch has already started for this
	     * URL. */
	    protoUndispatch(fd, astm->url, astm->entry);
	}
	storeUnlockObject(astm->entry);
    }
    /* If we have a read handler, we were reading in the get/post URL 
     * and don't have to deallocate the icpreadWrite buffer */
    handler = NULL;
    client_data = NULL;
    comm_get_select_handler(fd,
	COMM_SELECT_READ,
	(PF *) & handler,
	(caddr_t *) & client_data);
    if ((handler != NULL) && (client_data != NULL)) {
	rw_state = (icpReadWriteData *) client_data;
	/*
	 * the correct pointer for free is astm->url, NOT rw_state->buf
	 */
	safe_free(rw_state);
    }
    icpCloseAndFree(fd, astm, __LINE__);
}

/* Handle a new connection on ascii input socket. */
int asciiHandleConn(sock, notused)
     int sock;
     caddr_t notused;
{
    int fd = -1;
    int lft = -1;
    icpStateData *astm = NULL;
    struct sockaddr_in peer;
    struct sockaddr_in me;

    if ((fd = comm_accept(sock, &peer, &me)) < 0) {
	debug(12, 1, "asciiHandleConn: FD %d: accept failure: %s\n",
	    sock, xstrerror());
	comm_set_select_handler(sock, COMM_SELECT_READ, asciiHandleConn, 0);
	return -1;
    }
    /* set the hardwired lifetime */
    lft = comm_set_fd_lifetime(fd, getClientLifetime());
    nconn++;

    debug(12, 4, "asciiHandleConn: FD %d: accepted (lifetime %d).\n", fd, lft);
    fd_note(fd, inet_ntoa(peer.sin_addr));

    if (ip_access_check(peer.sin_addr, proxy_ip_acl) == IP_DENY
	&& ip_access_check(peer.sin_addr, accel_ip_acl) == IP_DENY) {
	astm = (icpStateData *) xcalloc(1, sizeof(icpStateData));
	debug(12, 2, "asciiHandleConn: %s: Access denied.\n",
	    inet_ntoa(peer.sin_addr));
	astm->log_type = LOG_TCP_DENIED;
	sprintf(tmp_error_buf,
	    "ACCESS DENIED\n\nYour IP address (%s) is not authorized to access cached at %s.\n\n",
	    inet_ntoa(peer.sin_addr),
	    getMyHostname());
	astm->buf = xstrdup(tmp_error_buf);
	astm->ptr_to_4k_page = NULL;
	icpWrite(fd,
	    astm->buf,
	    strlen(tmp_error_buf),
	    30,
	    icpSendERRORComplete,
	    (caddr_t) astm);
    } else {
	astm = (icpStateData *) xcalloc(1, sizeof(icpStateData));
	astm->url = (char *) xcalloc(ASCII_INBUF_SIZE, 1);
	astm->header.shostid = htonl(peer.sin_addr.s_addr);
	astm->peer = peer;
	astm->me = me;
	comm_set_select_handler(fd,
	    COMM_SELECT_LIFETIME,
	    (PF) asciiConnLifetimeHandle,
	    (caddr_t) astm);
	icpRead(fd,
	    FALSE,
	    astm->url,
	    ASCII_INBUF_SIZE - 1,
	    30,
	    asciiProcessInput,
	    (caddr_t) astm);
    }
    comm_set_select_handler(sock,
	COMM_SELECT_READ,
	asciiHandleConn,
	0);
    return 0;
}

icpUdpData *AppendUdp(item, head)
     icpUdpData *item;
     icpUdpData *head;
{
    item->next = NULL;
    if (head == NULL) {
	head = item;
	tail = head;
    } else if (tail == head) {
	tail = item;
	head->next = tail;
    } else {
	tail->next = item;
	tail = item;
    }
    return (head);
}

static void CheckQuickAbort(astm)
     icpStateData *astm;
{
    if ((getQuickAbort()
	    || !proto_cachable(astm->url, astm->type, astm->mime_hdr))
	&& (astm->entry->lock_count == 1)
	&& (astm->entry->status != STORE_OK)) {
	BIT_SET(astm->entry->flag, CLIENT_ABORT_REQUEST);
	BIT_SET(astm->entry->flag, RELEASE_REQUEST);
	astm->log_type = ERR_CLIENT_ABORT;
    }
}

/* $Id$ */

/*
 * DEBUG: Section 11          http: HTTP
 */

#include "squid.h"

#define HTTP_DELETE_GAP   (1<<18)

struct {
    int parsed;
    int date;
    int lm;
    int exp;
    int clen;
    int ctype;
} ReplyHeaderStats;

static int httpStateFree(fd, httpState)
     int fd;
     HttpStateData *httpState;
{
    if (httpState == NULL)
	return 1;
    storeUnlockObject(httpState->entry);
    if (httpState->reply_hdr) {
	put_free_8k_page(httpState->reply_hdr);
	httpState->reply_hdr = NULL;
    }
    if (httpState->reqbuf && httpState->buf_type == BUF_TYPE_8K) {
	put_free_8k_page(httpState->reqbuf);
	httpState->reqbuf = NULL;
    } else {
	safe_free(httpState->reqbuf)
    }
    requestUnlink(httpState->request);
    xfree(httpState);
    return 0;
}

int httpCachable(url, method)
     char *url;
     int method;
{
    wordlist *p = NULL;

    /* GET and HEAD are cachable. Others are not. */
    if (method != METHOD_GET && method != METHOD_HEAD)
	return 0;

    /* scan stop list */
    for (p = getHttpStoplist(); p; p = p->next) {
	if (strstr(url, p->key))
	    return 0;
    }

    /* else cachable */
    return 1;
}

/* This will be called when timeout on read. */
static void httpReadReplyTimeout(fd, httpState)
     int fd;
     HttpStateData *httpState;
{
    StoreEntry *entry = NULL;

    entry = httpState->entry;
    debug(11, 4, "httpReadReplyTimeout: FD %d: <URL:%s>\n", fd, entry->url);
    squid_error_entry(entry, ERR_READ_TIMEOUT, NULL);
    comm_set_select_handler(fd, COMM_SELECT_READ, 0, 0);
    comm_close(fd);
}

/* This will be called when socket lifetime is expired. */
static void httpLifetimeExpire(fd, httpState)
     int fd;
     HttpStateData *httpState;
{
    StoreEntry *entry = NULL;

    entry = httpState->entry;
    debug(11, 4, "httpLifeTimeExpire: FD %d: <URL:%s>\n", fd, entry->url);

    squid_error_entry(entry, ERR_LIFETIME_EXP, NULL);
    comm_set_select_handler(fd, COMM_SELECT_READ | COMM_SELECT_WRITE, 0, 0);
    comm_close(fd);
}

/* This object can be cached for a long time */
static void httpMakePublic(entry)
     StoreEntry *entry;
{
    entry->expires = squid_curtime + ttlSet(entry);
    if (BIT_TEST(entry->flag, CACHABLE))
	storeSetPublicKey(entry);
}

/* This object should never be cached at all */
static void httpMakePrivate(entry)
     StoreEntry *entry;
{
    storeSetPrivateKey(entry);
    storeExpireNow(entry);
    BIT_RESET(entry->flag, CACHABLE);
    storeReleaseRequest(entry);	/* delete object when not used */
}

/* This object may be negatively cached */
static void httpCacheNegatively(entry)
     StoreEntry *entry;
{
    entry->expires = squid_curtime + getNegativeTTL();
    if (BIT_TEST(entry->flag, CACHABLE))
	storeSetPublicKey(entry);
    /* XXX: mark object "not to store on disk"? */
}


/* Build a reply structure from HTTP reply headers */
void httpParseHeaders(buf, reply)
     char *buf;
     struct _http_reply *reply;
{
    char *headers = NULL;
    char *t = NULL;
    char *s = NULL;

    ReplyHeaderStats.parsed++;
    headers = xstrdup(buf);
    t = strtok(headers, "\n");
    while (t) {
	s = t + strlen(t);
	while (*s == '\r')
	    *s-- = '\0';
	if (!strncasecmp(t, "HTTP", 4)) {
	    sscanf(t + 1, "%lf", &reply->version);
	    if ((t = strchr(t, ' '))) {
		t++;
		reply->code = atoi(t);
	    }
	} else if (!strncasecmp(t, "Content-type:", 13)) {
	    if ((t = strchr(t, ' '))) {
		t++;
		strncpy(reply->content_type, t, HTTP_REPLY_FIELD_SZ - 1);
		ReplyHeaderStats.ctype++;
	    }
	} else if (!strncasecmp(t, "Content-length:", 15)) {
	    if ((t = strchr(t, ' '))) {
		t++;
		reply->content_length = atoi(t);
		ReplyHeaderStats.clen++;
	    }
	} else if (!strncasecmp(t, "Date:", 5)) {
	    if ((t = strchr(t, ' '))) {
		t++;
		strncpy(reply->date, t, HTTP_REPLY_FIELD_SZ - 1);
		ReplyHeaderStats.date++;
	    }
	} else if (!strncasecmp(t, "Expires:", 8)) {
	    if ((t = strchr(t, ' '))) {
		t++;
		strncpy(reply->expires, t, HTTP_REPLY_FIELD_SZ - 1);
		ReplyHeaderStats.exp++;
	    }
	} else if (!strncasecmp(t, "Last-Modified:", 14)) {
	    if ((t = strchr(t, ' '))) {
		t++;
		strncpy(reply->last_modified, t, HTTP_REPLY_FIELD_SZ - 1);
		ReplyHeaderStats.lm++;
	    }
	}
	t = strtok(NULL, "\n");
    }
    safe_free(headers);
}


void httpProcessReplyHeader(httpState, buf, size)
     HttpStateData *httpState;
     char *buf;			/* chunk just read by httpReadReply() */
     int size;
{
    char *t = NULL;
    StoreEntry *entry = httpState->entry;
    int room;
    int hdr_len;
    struct _http_reply *reply = NULL;

    debug(11, 3, "httpProcessReplyHeader: key '%s'\n", entry->key);

    if (httpState->reply_hdr == NULL) {
	httpState->reply_hdr = get_free_8k_page();
	memset(httpState->reply_hdr, '\0', 8192);
    }
    if (httpState->reply_hdr_state == 0) {
	hdr_len = strlen(httpState->reply_hdr);
	room = 8191 - hdr_len;
	strncat(httpState->reply_hdr, buf, room < size ? room : size);
	hdr_len += room < size ? room : size;
	if (hdr_len > 4 && strncmp(httpState->reply_hdr, "HTTP/", 5)) {
	    debug(11, 3, "httpProcessReplyHeader: Non-HTTP-compliant header: '%s'\n", entry->key);
	    httpState->reply_hdr_state += 2;
	    return;
	}
	/* Find the end of the headers */
	t = mime_headers_end(httpState->reply_hdr);
	if (!t)
	    /* XXX: Here we could check for buffer overflow... */
	    return;		/* headers not complete */
	/* Cut after end of headers */
	*t = '\0';
	reply = entry->mem_obj->reply;
	reply->hdr_sz = t - httpState->reply_hdr;
	debug(11, 7, "httpProcessReplyHeader: hdr_sz = %d\n", reply->hdr_sz);
	httpState->reply_hdr_state++;
    }
    if (httpState->reply_hdr_state == 1) {
	httpState->reply_hdr_state++;
	debug(11, 9, "GOT HTTP REPLY HDR:\n---------\n%s\n----------\n",
	    httpState->reply_hdr);
	/* Parse headers into reply structure */
	httpParseHeaders(httpState->reply_hdr, reply);
	/* Check if object is cacheable or not based on reply code */
	if (reply->code)
	    debug(11, 3, "httpProcessReplyHeader: HTTP CODE: %d\n", reply->code);
	switch (reply->code) {
	    /* Responses that are cacheable */
	case 200:		/* OK */
	case 203:		/* Non-Authoritative Information */
	case 300:		/* Multiple Choices */
	case 301:		/* Moved Permanently */
	case 410:		/* Gone */
	    /* These can be cached for a long time */
	    httpMakePublic(entry);
	    break;
	    /* Responses that only are cacheable if the server says so */
	case 302:		/* Moved temporarily */
	    if (*reply->expires)
		httpMakePublic(entry);
	    else
		httpMakePrivate(entry);
	    break;
	    /* Errors can be negatively cached */
	case 204:		/* No Content */
	case 305:		/* Use Proxy (proxy redirect) */
	case 400:		/* Bad Request */
	case 403:		/* Forbidden */
	case 404:		/* Not Found */
	case 405:		/* Method Now Allowed */
	case 414:		/* Request-URI Too Long */
	case 500:		/* Internal Server Error */
	case 501:		/* Not Implemented */
	case 502:		/* Bad Gateway */
	case 503:		/* Service Unavailable */
	case 504:		/* Gateway Timeout */
	    if (*reply->expires)
		httpMakePublic(entry);
	    else
		httpCacheNegatively(entry);
	    break;
	    /* Some responses can never be cached */
	case 303:		/* See Other */
	case 304:		/* Not Modified */
	case 401:		/* Unauthorized */
	case 407:		/* Proxy Authentication Required */
	default:		/* Unknown status code */
	    httpMakePrivate(entry);
	    break;
	}
    }
}


/* This will be called when data is ready to be read from fd.  Read until
 * error or connection closed. */
/* XXX this function is too long! */
static void httpReadReply(fd, httpState)
     int fd;
     HttpStateData *httpState;
{
    static char buf[SQUID_TCP_SO_RCVBUF];
    int len;
    int bin;
    int clen;
    int off;
    StoreEntry *entry = NULL;

    entry = httpState->entry;
    if (entry->flag & DELETE_BEHIND && !storeClientWaiting(entry)) {
	/* we can terminate connection right now */
	squid_error_entry(entry, ERR_NO_CLIENTS_BIG_OBJ, NULL);
	comm_close(fd);
	return;
    }
    /* check if we want to defer reading */
    clen = entry->mem_obj->e_current_len;
    off = storeGetLowestReaderOffset(entry);
    if ((clen - off) > HTTP_DELETE_GAP) {
	if (entry->flag & CLIENT_ABORT_REQUEST) {
	    squid_error_entry(entry, ERR_CLIENT_ABORT, NULL);
	    comm_close(fd);
	    return;
	}
	IOStats.Http.reads_deferred++;
	debug(11, 3, "httpReadReply: Read deferred for Object: %s\n",
	    entry->url);
	debug(11, 3, "                Current Gap: %d bytes\n", clen - off);
	/* reschedule, so it will be automatically reactivated
	 * when Gap is big enough. */
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) httpReadReply,
	    (void *) httpState);
	/* disable read timeout until we are below the GAP */
	comm_set_select_handler_plus_timeout(fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) NULL,
	    (void *) NULL,
	    (time_t) 0);
	comm_set_fd_lifetime(fd, 3600);		/* limit during deferring */
	/* dont try reading again for a while */
	comm_set_stall(fd, getStallDelay());
	return;
    }
    errno = 0;
    IOStats.Http.reads++;
    len = read(fd, buf, SQUID_TCP_SO_RCVBUF);
    debug(11, 5, "httpReadReply: FD %d: len %d.\n", fd, len);
    comm_set_fd_lifetime(fd, 86400);	/* extend after good read */
    if (len > 0) {
	for (clen = len - 1, bin = 0; clen; bin++)
	    clen >>= 1;
	IOStats.Http.read_hist[bin]++;
    }
    if (len < 0) {
	debug(11, 2, "httpReadReply: FD %d: read failure: %s.\n",
	    fd, xstrerror());
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    /* reinstall handlers */
	    /* XXX This may loop forever */
	    comm_set_select_handler(fd, COMM_SELECT_READ,
		(PF) httpReadReply, (void *) httpState);
	    comm_set_select_handler_plus_timeout(fd, COMM_SELECT_TIMEOUT,
		(PF) httpReadReplyTimeout, (void *) httpState, getReadTimeout());
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
	storeComplete(entry);
	comm_close(fd);
    } else if ((entry->mem_obj->e_current_len + len) > getHttpMax() &&
	!(entry->flag & DELETE_BEHIND)) {
	/*  accept data, but start to delete behind it */
	storeStartDeleteBehind(entry);
	storeAppend(entry, buf, len);
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) httpReadReply,
	    (void *) httpState);
	comm_set_select_handler_plus_timeout(fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) httpReadReplyTimeout,
	    (void *) httpState, getReadTimeout());
    } else if (entry->flag & CLIENT_ABORT_REQUEST) {
	/* append the last bit of info we get */
	storeAppend(entry, buf, len);
	squid_error_entry(entry, ERR_CLIENT_ABORT, NULL);
	comm_close(fd);
    } else {
	storeAppend(entry, buf, len);
	if (httpState->reply_hdr_state < 2 && len > 0)
	    httpProcessReplyHeader(httpState, buf, len);
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) httpReadReply,
	    (void *) httpState);
	comm_set_select_handler_plus_timeout(fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) httpReadReplyTimeout,
	    (void *) httpState,
	    getReadTimeout());
    }
}

/* This will be called when request write is complete. Schedule read of
 * reply. */
static void httpSendComplete(fd, buf, size, errflag, data)
     int fd;
     char *buf;
     int size;
     int errflag;
     void *data;
{
    HttpStateData *httpState = data;
    StoreEntry *entry = NULL;

    entry = httpState->entry;
    debug(11, 5, "httpSendComplete: FD %d: size %d: errflag %d.\n",
	fd, size, errflag);

    if (httpState->reqbuf && httpState->buf_type == BUF_TYPE_8K) {
	put_free_8k_page(httpState->reqbuf);
	httpState->reqbuf = NULL;
    } else {
	safe_free(httpState->reqbuf);
    }

    if (errflag) {
	squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	comm_close(fd);
	return;
    } else {
	/* Schedule read reply. */
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) httpReadReply,
	    (void *) httpState);
	comm_set_select_handler_plus_timeout(fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) httpReadReplyTimeout,
	    (void *) httpState,
	    getReadTimeout());
	comm_set_fd_lifetime(fd, 86400);	/* extend disable lifetime */
    }
}

/* This will be called when connect completes. Write request. */
static void httpSendRequest(fd, httpState)
     int fd;
     HttpStateData *httpState;
{
    char *xbuf = NULL;
    char *ybuf = NULL;
    char *buf = NULL;
    char *t = NULL;
    char *post_buf = NULL;
    static char *crlf = "\r\n";
    static char *VIA_PROXY_TEXT = "via Squid Cache version";
    int len = 0;
    int buflen;
    int cfd = -1;
    request_t *req = httpState->request;
    char *Method = RequestMethodStr[req->method];

    debug(11, 5, "httpSendRequest: FD %d: httpState %p.\n", fd, httpState);
    buflen = strlen(Method) + strlen(req->urlpath);
    if (httpState->req_hdr)
	buflen += strlen(httpState->req_hdr);
    buflen += 512;		/* lots of extra */

    if (req->method == METHOD_POST && httpState->req_hdr) {
	if ((t = mime_headers_end(httpState->req_hdr))) {
	    post_buf = xstrdup(t);
	    *t = '\0';
	}
    }
    if (buflen < DISK_PAGE_SIZE) {
	httpState->reqbuf = get_free_8k_page();
	memset(httpState->reqbuf, '\0', buflen);
	httpState->buf_type = BUF_TYPE_8K;
    } else {
	httpState->reqbuf = xcalloc(buflen, 1);
	httpState->buf_type = BUF_TYPE_MALLOC;
    }
    buf = httpState->reqbuf;

    sprintf(buf, "%s %s HTTP/1.0\r\n",
	Method,
	*req->urlpath ? req->urlpath : "/");
    len = strlen(buf);
    if (httpState->req_hdr) {	/* we have to parse the request header */
	xbuf = xstrdup(httpState->req_hdr);
	for (t = strtok(xbuf, crlf); t; t = strtok(NULL, crlf)) {
	    if (strncasecmp(t, "User-Agent:", 11) == 0) {
		ybuf = (char *) get_free_4k_page();
		memset(ybuf, '\0', SM_PAGE_SIZE);
		sprintf(ybuf, "%s %s %s", t, VIA_PROXY_TEXT, version_string);
		t = ybuf;
	    }
	    if (len + (int) strlen(t) > buflen - 10)
		continue;
	    strcat(buf, t);
	    strcat(buf, crlf);
	    len += strlen(t) + 2;
	}
	xfree(xbuf);
	if (ybuf) {
	    put_free_4k_page(ybuf);
	    ybuf = NULL;
	}
    }
    /* Add Forwarded: header */
    ybuf = get_free_4k_page();
    if (httpState->entry->mem_obj)
	cfd = httpState->entry->mem_obj->fd_of_first_client;
    if (cfd < 0) {
	sprintf(ybuf, "%s\r\n", ForwardedBy);
    } else {
	sprintf(ybuf, "%s for %s\r\n", ForwardedBy, fd_table[cfd].ipaddr);
    }
    strcat(buf, ybuf);
    len += strlen(ybuf);
    put_free_4k_page(ybuf);
    ybuf = NULL;

    strcat(buf, crlf);
    len += 2;
    if (post_buf) {
	strcat(buf, post_buf);
	len += strlen(post_buf);
	xfree(post_buf);
    }
    debug(11, 6, "httpSendRequest: FD %d: buf '%s'\n", fd, buf);
    comm_write(fd,
	buf,
	len,
	30,
	httpSendComplete,
	httpState);
}

static void httpConnInProgress(fd, httpState)
     int fd;
     HttpStateData *httpState;
{
    StoreEntry *entry = httpState->entry;
    request_t *req = httpState->request;

    debug(11, 5, "httpConnInProgress: FD %d httpState=%p\n", fd, httpState);

    if (comm_connect(fd, req->host, req->port) != COMM_OK) {
	debug(11, 5, "httpConnInProgress: FD %d: %s\n", fd, xstrerror());
	switch (errno) {
	case EINPROGRESS:
	case EALREADY:
	    /* schedule this handler again */
	    comm_set_select_handler(fd,
		COMM_SELECT_WRITE,
		(PF) httpConnInProgress,
		(void *) httpState);
	    return;
	default:
	    squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	    comm_close(fd);
	    return;
	}
    }
    /* Call the real write handler, now that we're fully connected */
    comm_set_select_handler(fd, COMM_SELECT_WRITE,
	(PF) httpSendRequest, (void *) httpState);
}

int proxyhttpStart(e, url, entry)
     edge *e;
     char *url;
     StoreEntry *entry;
{
    int sock;
    int status;
    HttpStateData *httpState = NULL;
    request_t *request = NULL;

    debug(11, 3, "proxyhttpStart: \"%s %s\"\n",
	RequestMethodStr[entry->method], url);
    debug(11, 10, "proxyhttpStart: HTTP request header:\n%s\n",
	entry->mem_obj->mime_hdr);

    if (e->proxy_only)
	storeStartDeleteBehind(entry);

    /* Create socket. */
    sock = comm_open(COMM_NONBLOCKING, getTcpOutgoingAddr(), 0, url);
    if (sock == COMM_ERROR) {
	debug(11, 4, "proxyhttpStart: Failed because we're out of sockets.\n");
	squid_error_entry(entry, ERR_NO_FDS, xstrerror());
	return COMM_ERROR;
    }
    httpState = xcalloc(1, sizeof(HttpStateData));
    storeLockObject(httpState->entry = entry, NULL, NULL);
    httpState->req_hdr = entry->mem_obj->mime_hdr;
    request = get_free_request_t();
    httpState->request = requestLink(request);
    /* register the handler to free HTTP state data when the FD closes */
    comm_add_close_handler(sock,
	(PF) httpStateFree,
	(void *) httpState);

    request->method = entry->method;
    strncpy(request->host, e->host, SQUIDHOSTNAMELEN);
    request->port = e->http_port;
    strncpy(request->urlpath, url, MAX_URL);

    /* check if IP is already in cache. It must be. 
     * It should be done before this route is called. 
     * Otherwise, we cannot check return code for connect. */
    if (!ipcache_gethostbyname(request->host, IP_BLOCKING_LOOKUP)) {
	debug(11, 4, "proxyhttpstart: Called without IP entry in ipcache. OR lookup failed.\n");
	squid_error_entry(entry, ERR_DNS_FAIL, dns_error_message);
	comm_close(sock);
	return COMM_ERROR;
    }
    /* Open connection. */
    if ((status = comm_connect(sock, request->host, request->port))) {
	if (status != EINPROGRESS) {
	    squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	    comm_close(sock);
	    e->last_fail_time = squid_curtime;
	    e->neighbor_up = 0;
	    return COMM_ERROR;
	} else {
	    debug(11, 5, "proxyhttpStart: FD %d: EINPROGRESS.\n", sock);
	    comm_set_select_handler(sock, COMM_SELECT_LIFETIME,
		(PF) httpLifetimeExpire, (void *) httpState);
	    comm_set_select_handler(sock, COMM_SELECT_WRITE,
		(PF) httpConnInProgress, (void *) httpState);
	    return COMM_OK;
	}
    }
    /* Install connection complete handler. */
    fd_note(sock, entry->url);
    comm_set_select_handler(sock, COMM_SELECT_LIFETIME,
	(PF) httpLifetimeExpire, (void *) httpState);
    comm_set_select_handler(sock, COMM_SELECT_WRITE,
	(PF) httpSendRequest, (void *) httpState);
    return COMM_OK;
}

int httpStart(unusedfd, url, request, req_hdr, entry)
     int unusedfd;
     char *url;
     request_t *request;
     char *req_hdr;
     StoreEntry *entry;
{
    /* Create state structure. */
    int sock, status;
    HttpStateData *httpState = NULL;

    debug(11, 3, "httpStart: \"%s %s\"\n",
	RequestMethodStr[request->method], url);
    debug(11, 10, "httpStart: req_hdr '%s'\n", req_hdr);

    /* Create socket. */
    sock = comm_open(COMM_NONBLOCKING, getTcpOutgoingAddr(), 0, url);
    if (sock == COMM_ERROR) {
	debug(11, 4, "httpStart: Failed because we're out of sockets.\n");
	squid_error_entry(entry, ERR_NO_FDS, xstrerror());
	return COMM_ERROR;
    }
    httpState = xcalloc(1, sizeof(HttpStateData));
    storeLockObject(httpState->entry = entry, NULL, NULL);
    httpState->req_hdr = req_hdr;
    httpState->request = requestLink(request);
    comm_add_close_handler(sock,
	(PF) httpStateFree,
	(void *) httpState);

    /* check if IP is already in cache. It must be. 
     * It should be done before this route is called. 
     * Otherwise, we cannot check return code for connect. */
    if (!ipcache_gethostbyname(request->host, 0)) {
	debug(11, 4, "httpstart: Called without IP entry in ipcache. OR lookup failed.\n");
	squid_error_entry(entry, ERR_DNS_FAIL, dns_error_message);
	comm_close(sock);
	return COMM_ERROR;
    }
    /* Open connection. */
    if ((status = comm_connect(sock, request->host, request->port))) {
	if (status != EINPROGRESS) {
	    squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	    comm_close(sock);
	    return COMM_ERROR;
	} else {
	    debug(11, 5, "httpStart: FD %d: EINPROGRESS.\n", sock);
	    comm_set_select_handler(sock, COMM_SELECT_LIFETIME,
		(PF) httpLifetimeExpire, (void *) httpState);
	    comm_set_select_handler(sock, COMM_SELECT_WRITE,
		(PF) httpConnInProgress, (void *) httpState);
	    return COMM_OK;
	}
    }
    /* Install connection complete handler. */
    fd_note(sock, entry->url);
    comm_set_select_handler(sock, COMM_SELECT_LIFETIME,
	(PF) httpLifetimeExpire, (void *) httpState);
    comm_set_select_handler(sock, COMM_SELECT_WRITE,
	(PF) httpSendRequest, (void *) httpState);
    return COMM_OK;
}

void httpReplyHeaderStats(entry)
     StoreEntry *entry;
{
    storeAppendPrintf(entry, open_bracket);
    storeAppendPrintf(entry, "{HTTP Reply Headers}\n");
    storeAppendPrintf(entry, "{Headers parsed: %d}\n",
	ReplyHeaderStats.parsed);
    storeAppendPrintf(entry, "{          Date: %d}\n",
	ReplyHeaderStats.date);
    storeAppendPrintf(entry, "{ Last-Modified: %d}\n",
	ReplyHeaderStats.lm);
    storeAppendPrintf(entry, "{       Expires: %d}\n",
	ReplyHeaderStats.exp);
    storeAppendPrintf(entry, "{  Content-Type: %d}\n",
	ReplyHeaderStats.ctype);
    storeAppendPrintf(entry, "{Content-Length: %d}\n",
	ReplyHeaderStats.clen);
    storeAppendPrintf(entry, close_bracket);
}

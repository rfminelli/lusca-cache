/* $Id$ */

/*
 * DEBUG: Section 9           ftp: FTP
 */

#include "squid.h"

#define FTP_DELETE_GAP  (1<<18)
#define MAGIC_MARKER    "\004\004\004"	/* No doubt this should be more configurable */
#define MAGIC_MARKER_SZ 3

static char ftpASCII[] = "A";
static char ftpBinary[] = "I";
static int ftpget_server_read = -1;
static int ftpget_server_write = -1;
static u_short ftpget_port = 0;

typedef struct _Ftpdata {
    StoreEntry *entry;
    request_t *request;
    char user[MAX_URL];
    char password[MAX_URL];
    char *reply_hdr;
    int ftp_fd;
    char *icp_page_ptr;		/* Used to send proxy-http request: 
				 * put_free_8k_page(me) if the lifetime
				 * expires */
    int got_marker;		/* denotes end of successful request */
    int reply_hdr_state;
} FtpData;


/* Local functions */
static int ftpStateFree _PARAMS((int fd, FtpData * ftpState));
static void ftpProcessReplyHeader _PARAMS((FtpData * data, char *buf, int size));
static void ftpServerClosed _PARAMS((int fd, void *nodata));
static void ftp_login_parser _PARAMS((char *login, FtpData * data));

/* Global functions not declared in ftp.h */
void ftpLifetimeExpire _PARAMS((int fd, FtpData * data));
int ftpReadReply _PARAMS((int fd, FtpData * data));
void ftpSendComplete _PARAMS((int fd, char *buf, int size, int errflag, void *ftpData));
void ftpSendRequest _PARAMS((int fd, FtpData * data));
void ftpConnInProgress _PARAMS((int fd, FtpData * data));
void ftpServerClose _PARAMS((void));

static int ftpStateFree(fd, ftpState)
     int fd;
     FtpData *ftpState;
{
    if (ftpState == NULL)
	return 1;
    storeUnlockObject(ftpState->entry);
    if (ftpState->reply_hdr) {
	put_free_8k_page(ftpState->reply_hdr);
	ftpState->reply_hdr = NULL;
    }
    if (ftpState->icp_page_ptr) {
	put_free_8k_page(ftpState->icp_page_ptr);
	ftpState->icp_page_ptr = NULL;
    }
    requestUnlink(ftpState->request);
    xfree(ftpState);
    return 0;
}

static void ftp_login_parser(login, data)
     char *login;
     FtpData *data;
{
    char *user = data->user;
    char *password = data->password;
    char *s = NULL;

    strcpy(user, login);
    s = strchr(user, ':');
    if (s) {
	*s = 0;
	strcpy(password, s + 1);
    } else {
	strcpy(password, "");
    }

    if (!*user && !*password) {
	strcpy(user, "anonymous");
	strcpy(password, getFtpUser());
    }
}

int ftpCachable(url)
     char *url;
{
    wordlist *p = NULL;

    /* scan stop list */
    for (p = getFtpStoplist(); p; p = p->next) {
	if (strstr(url, p->key))
	    return 0;
    }

    /* else cachable */
    return 1;
}

/* This will be called when socket lifetime is expired. */
void ftpLifetimeExpire(fd, data)
     int fd;
     FtpData *data;
{
    StoreEntry *entry = NULL;
    entry = data->entry;
    debug(9, 4, "ftpLifeTimeExpire: FD %d: <URL:%s>\n", fd, entry->url);
    squid_error_entry(entry, ERR_LIFETIME_EXP, NULL);
    comm_close(fd);
}


/* This is too much duplicated code from httpProcessReplyHeader.  Only
 * difference is FtpData vs HttpData. */
static void ftpProcessReplyHeader(data, buf, size)
     FtpData *data;
     char *buf;			/* chunk just read by ftpReadReply() */
     int size;
{
    char *t = NULL;
    StoreEntry *entry = data->entry;
    int room;
    int hdr_len;
    struct _http_reply *reply = NULL;

    debug(11, 3, "ftpProcessReplyHeader: key '%s'\n", entry->key);

    if (data->reply_hdr == NULL) {
	data->reply_hdr = get_free_8k_page();
	memset(data->reply_hdr, '\0', 8192);
    }
    if (data->reply_hdr_state == 0) {
	hdr_len = strlen(data->reply_hdr);
	room = 8191 - hdr_len;
	strncat(data->reply_hdr, buf, room < size ? room : size);
	hdr_len += room < size ? room : size;
	if (hdr_len > 4 && strncmp(data->reply_hdr, "HTTP/", 5)) {
	    debug(11, 3, "ftpProcessReplyHeader: Non-HTTP-compliant header: '%s'\n", entry->key);
	    data->reply_hdr_state += 2;
	    return;
	}
	/* Find the end of the headers */
	t = mime_headers_end(data->reply_hdr);
	if (!t)
	    return;		/* headers not complete */
	/* Cut after end of headers */
	*t = '\0';
	reply = entry->mem_obj->reply;
	reply->hdr_sz = t - data->reply_hdr;
	debug(11, 7, "ftpProcessReplyHeader: hdr_sz = %d\n", reply->hdr_sz);
	data->reply_hdr_state++;
    }
    if (data->reply_hdr_state == 1) {
	data->reply_hdr_state++;
	debug(11, 9, "GOT HTTP REPLY HDR:\n---------\n%s\n----------\n",
	    data->reply_hdr);
	/* Parse headers into reply structure */
	httpParseHeaders(data->reply_hdr, reply);
	/* Check if object is cacheable or not based on reply code */
	if (reply->code)
	    debug(11, 3, "ftpProcessReplyHeader: HTTP CODE: %d\n", reply->code);
	switch (reply->code) {
	case 200:		/* OK */
	case 203:		/* Non-Authoritative Information */
	case 300:		/* Multiple Choices */
	case 301:		/* Moved Permanently */
	case 410:		/* Gone */
	    /* These can be cached for a long time, make the key public */
	    entry->expires = squid_curtime + ttlSet(entry);
	    if (BIT_TEST(entry->flag, CACHABLE))
		storeSetPublicKey(entry);
	    break;
	case 302:		/* Moved Temporarily */
	case 304:		/* Not Modified */
	case 401:		/* Unauthorized */
	case 407:		/* Proxy Authentication Required */
	    /* These should never be cached at all */
	    storeSetPrivateKey(entry);
	    storeExpireNow(entry);
	    BIT_RESET(entry->flag, CACHABLE);
	    storeReleaseRequest(entry);
	    break;
	default:
	    /* These can be negative cached, make key public */
	    entry->expires = squid_curtime + getNegativeTTL();
	    if (BIT_TEST(entry->flag, CACHABLE))
		storeSetPublicKey(entry);
	    break;
	}
    }
}


/* This will be called when data is ready to be read from fd.  Read until
 * error or connection closed. */
int ftpReadReply(fd, data)
     int fd;
     FtpData *data;
{
    static char buf[SQUID_TCP_SO_RCVBUF];
    int len;
    int clen;
    int off;
    int bin;
    StoreEntry *entry = NULL;

    entry = data->entry;
    if (entry->flag & DELETE_BEHIND && !storeClientWaiting(entry)) {
	/* we can terminate connection right now */
	squid_error_entry(entry, ERR_NO_CLIENTS_BIG_OBJ, NULL);
	comm_close(fd);
	return 0;
    }
    /* check if we want to defer reading */
    clen = entry->mem_obj->e_current_len;
    off = storeGetLowestReaderOffset(entry);
    if ((clen - off) > FTP_DELETE_GAP) {
	if (entry->flag & CLIENT_ABORT_REQUEST) {
	    squid_error_entry(entry, ERR_CLIENT_ABORT, NULL);
	    comm_close(fd);
	}
	IOStats.Ftp.reads_deferred++;
	debug(11, 3, "ftpReadReply: Read deferred for Object: %s\n",
	    entry->url);
	debug(11, 3, "                Current Gap: %d bytes\n", clen - off);
	/* reschedule, so it will be automatically reactivated
	 * when Gap is big enough. */
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) ftpReadReply,
	    (void *) data);
	/* NOTE there is no read timeout handler to disable */
	/* dont try reading again for a while */
	comm_set_stall(fd, getStallDelay());
	return 0;
    }
    errno = 0;
    IOStats.Ftp.reads++;
    len = read(fd, buf, SQUID_TCP_SO_RCVBUF);
    debug(9, 5, "ftpReadReply: FD %d, Read %d bytes\n", fd, len);
    if (len > 0) {
	for (clen = len - 1, bin = 0; clen; bin++)
	    clen >>= 1;
	IOStats.Ftp.read_hist[bin]++;
    }
    if (len < 0) {
	debug(9, 1, "ftpReadReply: read error: %s\n", xstrerror());
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    /* reinstall handlers */
	    /* XXX This may loop forever */
	    comm_set_select_handler(fd, COMM_SELECT_READ,
		(PF) ftpReadReply, (void *) data);
	    /* note there is no ftpReadReplyTimeout.  Timeouts are handled
	     * by `ftpget'. */
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
	if (!data->got_marker) {
	    /* If we didn't see the magic marker, assume the transfer
	     * failed and arrange so the object gets ejected and
	     * never gets to disk. */
	    debug(9, 1, "ftpReadReply: Purging '%s'\n", entry->url);
	    entry->expires = squid_curtime + getNegativeTTL();
	    BIT_RESET(entry->flag, CACHABLE);
	    storeReleaseRequest(entry);
	} else if (!(entry->flag & DELETE_BEHIND)) {
	    entry->expires = squid_curtime + ttlSet(entry);
	}
	/* update fdstat and fdtable */
	storeComplete(entry);
	comm_close(fd);
    } else if (((entry->mem_obj->e_current_len + len) > getFtpMax()) &&
	!(entry->flag & DELETE_BEHIND)) {
	/*  accept data, but start to delete behind it */
	storeStartDeleteBehind(entry);
	storeAppend(entry, buf, len);
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) ftpReadReply,
	    (void *) data);
    } else if (entry->flag & CLIENT_ABORT_REQUEST) {
	/* append the last bit of info we get */
	storeAppend(entry, buf, len);
	squid_error_entry(entry, ERR_CLIENT_ABORT, NULL);
	comm_close(fd);
    } else {
	/* check for a magic marker at the end of the read */
	data->got_marker = 0;
	if (len >= MAGIC_MARKER_SZ) {
	    if (!memcmp(MAGIC_MARKER, buf + len - MAGIC_MARKER_SZ, MAGIC_MARKER_SZ)) {
		data->got_marker = 1;
		len -= MAGIC_MARKER_SZ;
	    }
	}
	storeAppend(entry, buf, len);
	if (data->reply_hdr_state < 2 && len > 0)
	    ftpProcessReplyHeader(data, buf, len);
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) ftpReadReply,
	    (void *) data);
	comm_set_select_handler_plus_timeout(fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) ftpLifetimeExpire,
	    (void *) data,
	    getReadTimeout());
    }
    return 0;
}

void ftpSendComplete(fd, buf, size, errflag, data)
     int fd;
     char *buf;
     int size;
     int errflag;
     void *data;
{
    FtpData *ftpState = (FtpData *) data;
    StoreEntry *entry = NULL;

    entry = ftpState->entry;
    debug(9, 5, "ftpSendComplete: FD %d: size %d: errflag %d.\n",
	fd, size, errflag);

    if (buf) {
	put_free_8k_page(buf);	/* Allocated by ftpSendRequest. */
	buf = NULL;
    }
    ftpState->icp_page_ptr = NULL;	/* So lifetime expire doesn't re-free */

    if (errflag) {
	squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	comm_close(fd);
	return;
    } else {
	comm_set_select_handler(ftpState->ftp_fd,
	    COMM_SELECT_READ,
	    (PF) ftpReadReply,
	    (void *) ftpState);
	comm_set_select_handler_plus_timeout(ftpState->ftp_fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) ftpLifetimeExpire,
	    (void *) ftpState, getReadTimeout());
    }
}

void ftpSendRequest(fd, data)
     int fd;
     FtpData *data;
{
    char *ext = NULL;
    ext_table_entry *e = NULL;
    int l;
    char *path = NULL;
    char *mode = NULL;
    char *buf = NULL;
    static char tbuf[BUFSIZ];
    static char opts[BUFSIZ];
    static char *space = " ";
    char *s = NULL;
    int got_timeout = 0;
    int got_negttl = 0;
    int buflen;

    debug(9, 5, "ftpSendRequest: FD %d\n", fd);

    buflen = strlen(data->request->urlpath) + 256;
    buf = (char *) get_free_8k_page();
    data->icp_page_ptr = buf;
    memset(buf, '\0', buflen);

    path = data->request->urlpath;
    l = strlen(path);
    if (path[l - 1] == '/')
	mode = ftpASCII;
    else {
	if ((ext = strrchr(path, '.')) != NULL) {
	    ext++;
	    mode = ((e = mime_ext_to_type(ext)) &&
		strncmp(e->mime_type, "text", 4) == 0) ? ftpASCII :
		ftpBinary;
	} else
	    mode = ftpBinary;
    }

    /* Start building the buffer ... */
    strcat(buf, getFtpProgram());
    strcat(buf, space);

    strncpy(opts, getFtpOptions(), BUFSIZ);
    for (s = strtok(opts, w_space); s; s = strtok(NULL, w_space)) {
	strcat(buf, s);
	strcat(buf, space);
	if (!strncmp(s, "-t", 2))
	    got_timeout = 1;
	if (!strncmp(s, "-n", 2))
	    got_negttl = 1;
    }
    if (!got_timeout) {
	sprintf(tbuf, "-t %d ", getReadTimeout());
	strcat(buf, tbuf);
    }
    if (!got_negttl) {
	sprintf(tbuf, "-n %d ", getNegativeTTL());
	strcat(buf, tbuf);
    }
    if (data->request->port) {
	sprintf(tbuf, "-P %d ", data->request->port);
	strcat(buf, tbuf);
    }
    strcat(buf, "-h ");		/* httpify */
    strcat(buf, "- ");		/* stdout */
    strcat(buf, data->request->host);
    strcat(buf, space);
    strcat(buf, *path ? path : "\"\"");
    strcat(buf, space);
    strcat(buf, mode);		/* A or I */
    strcat(buf, space);
    strcat(buf, *data->user ? data->user : "\"\"");
    strcat(buf, space);
    strcat(buf, *data->password ? data->password : "\"\"");
    strcat(buf, "\n");
    debug(9, 5, "ftpSendRequest: FD %d: buf '%s'\n", fd, buf);
    comm_write(fd,
	buf,
	strlen(buf),
	30,
	ftpSendComplete,
	(void *) data);
}

void ftpConnInProgress(fd, data)
     int fd;
     FtpData *data;
{
    StoreEntry *entry = data->entry;

    debug(9, 5, "ftpConnInProgress: FD %d\n", fd);

    if (comm_connect(fd, localhost, ftpget_port) != COMM_OK) {
	switch (errno) {
	case EINPROGRESS:
	case EALREADY:
	    /* schedule this handler again */
	    comm_set_select_handler(fd,
		COMM_SELECT_WRITE,
		(PF) ftpConnInProgress,
		(void *) data);
	    return;
	default:
	    squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	    comm_close(fd);
	    return;
	}
    }
    /* Call the real write handler, now that we're fully connected */
    comm_set_select_handler(fd,
	COMM_SELECT_WRITE,
	(PF) ftpSendRequest,
	(void *) data);
}


int ftpStart(unusedfd, url, request, entry)
     int unusedfd;
     char *url;
     request_t *request;
     StoreEntry *entry;
{
    FtpData *data = NULL;
    int status;

    debug(9, 3, "FtpStart: FD %d <URL:%s>\n", unusedfd, url);

    data = xcalloc(1, sizeof(FtpData));
    storeLockObject(data->entry = entry, NULL, NULL);
    data->request = requestLink(request);

    /* Parse login info. */
    ftp_login_parser(request->login, data);

    debug(9, 5, "FtpStart: FD %d, host=%s, path=%s, user=%s, passwd=%s\n",
	unusedfd, data->request->host, data->request->urlpath,
	data->user, data->password);

    data->ftp_fd = comm_open(COMM_NONBLOCKING,
	local_addr,
	0,
	url);
    if (data->ftp_fd == COMM_ERROR) {
	squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	ftpStateFree(-1, data);
	return COMM_ERROR;
    }
    /* Pipe/socket created ok */

    /* register close handler */
    comm_add_close_handler(data->ftp_fd,
	(PF) ftpStateFree,
	(void *) data);

    /* Now connect ... */
    if ((status = comm_connect(data->ftp_fd, localhost, ftpget_port))) {
	if (status != EINPROGRESS) {
	    squid_error_entry(entry, ERR_CONNECT_FAIL, xstrerror());
	    comm_close(data->ftp_fd);
	    return COMM_ERROR;
	} else {
	    debug(9, 5, "ftpStart: FD %d: EINPROGRESS.\n", data->ftp_fd);
	    comm_set_select_handler(data->ftp_fd, COMM_SELECT_LIFETIME,
		(PF) ftpLifetimeExpire, (void *) data);
	    comm_set_select_handler(data->ftp_fd, COMM_SELECT_WRITE,
		(PF) ftpConnInProgress, (void *) data);
	    return COMM_OK;
	}
    }
    fdstat_open(data->ftp_fd, FD_SOCKET);
    commSetNonBlocking(data->ftp_fd);
    (void) fd_note(data->ftp_fd, entry->url);

    /* Install connection complete handler. */
    fd_note(data->ftp_fd, entry->url);
    comm_set_select_handler(data->ftp_fd,
	COMM_SELECT_WRITE,
	(PF) ftpSendRequest,
	(void *) data);
    comm_set_fd_lifetime(data->ftp_fd,
	getClientLifetime());
    comm_set_select_handler(data->ftp_fd,
	COMM_SELECT_LIFETIME,
	(PF) ftpLifetimeExpire,
	(void *) data);
    return COMM_OK;
}

static void ftpServerClosed(fd, nodata)
     int fd;
     void *nodata;
{
    static time_t last_restart = 0;
    comm_close(fd);
    if (squid_curtime - last_restart < 2) {
	debug(9, 0, "ftpget server failing too rapidly\n");
	debug(9, 0, "WARNING: FTP access is disabled!\n");
	return;
    }
    last_restart = squid_curtime;
    debug(9, 1, "Restarting ftpget server...\n");
    (void) ftpInitialize();
}

void ftpServerClose()
{
    if (ftpget_server_read < 0)
	return;

    comm_set_select_handler(ftpget_server_read,
	COMM_SELECT_READ,
	(PF) NULL,
	(void *) NULL);
    fdstat_close(ftpget_server_read);
    close(ftpget_server_read);
    fdstat_close(ftpget_server_write);
    close(ftpget_server_write);
    ftpget_server_read = -1;
    ftpget_server_write = -1;
}


int ftpInitialize()
{
    int pid;
    int cfd;
    int squid_to_ftpget[2];
    int ftpget_to_squid[2];
    char pbuf[128];
    char *ftpget = getFtpProgram();
    struct sockaddr_in S;
    int len;

    if (pipe(squid_to_ftpget) < 0) {
	debug(9, 0, "ftpInitialize: pipe: %s\n", xstrerror());
	return -1;
    }
    if (pipe(ftpget_to_squid) < 0) {
	debug(9, 0, "ftpInitialize: pipe: %s\n", xstrerror());
	return -1;
    }
    cfd = comm_open(COMM_NOCLOEXEC,
	local_addr,
	0,
	"ftpget -S socket");
    if (cfd == COMM_ERROR) {
	debug(9, 0, "ftpInitialize: Failed to create socket\n");
	return -1;
    }
    len = sizeof(S);
    memset(&S, '\0', len);
    if (getsockname(cfd, (struct sockaddr *) &S, &len) < 0) {
	debug(9, 0, "ftpInitialize: getsockname: %s\n", xstrerror());
	comm_close(cfd);
	return -1;
    }
    ftpget_port = ntohs(S.sin_port);
    listen(cfd, FD_SETSIZE >> 2);
    if ((pid = fork()) < 0) {
	debug(9, 0, "ftpInitialize: fork: %s\n", xstrerror());
	comm_close(cfd);
	return -1;
    }
    if (pid != 0) {		/* parent */
	comm_close(cfd);
	close(squid_to_ftpget[0]);
	close(ftpget_to_squid[1]);
	fdstat_open(squid_to_ftpget[1], FD_PIPE);
	fdstat_open(ftpget_to_squid[0], FD_PIPE);
	fd_note(squid_to_ftpget[1], "ftpget -S");
	fd_note(ftpget_to_squid[0], "ftpget -S");
	fcntl(squid_to_ftpget[1], F_SETFD, 1);	/* set close-on-exec */
	fcntl(ftpget_to_squid[0], F_SETFD, 1);	/* set close-on-exec */
	/* if ftpget -S goes away, this handler should get called */
	comm_set_select_handler(ftpget_to_squid[0],
	    COMM_SELECT_READ,
	    (PF) ftpServerClosed,
	    (void *) NULL);
	ftpget_server_write = squid_to_ftpget[1];
	ftpget_server_read = ftpget_to_squid[0];
	return 0;
    }
    /* child */
    /* give up all extra priviligies */
    no_suid();
    /* set up stdin,stdout */
    dup2(squid_to_ftpget[0], 0);
    dup2(ftpget_to_squid[1], 1);
    dup2(fileno(debug_log), 2);
    close(squid_to_ftpget[0]);
    close(squid_to_ftpget[1]);
    close(ftpget_to_squid[0]);
    close(ftpget_to_squid[1]);
    dup2(cfd, 3);		/* pass listening socket to ftpget */
    /* inherit stdin,stdout,stderr */
    for (cfd = 4; cfd <= fdstat_biggest_fd(); cfd++)
	(void) close(cfd);
    sprintf(pbuf, "%d", ftpget_port);
    execlp(ftpget, ftpget, "-S", pbuf, NULL);
    debug(9, 0, "ftpInitialize: %s: %s\n", ftpget, xstrerror());
    _exit(1);
    return (1);			/* eliminate compiler warning */
}

/* $Id$ */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>		/* for WNOHANG */
#include <unistd.h>
#include <errno.h>

#include "ansihelp.h"
#include "comm.h"
#include "store.h"
#include "stat.h"
#include "url.h"
#include "mime.h"
#include "fdstat.h"
#include "cache_cf.h"
#include "ttl.h"
#include "util.h"
#include "ftp.h"
#include "icp.h"
#include "cached_error.h"

#define FTP_DELETE_GAP  (64*1024)

ftpget_thread *FtpgetThread = NULL;
ftpget_thread **FtpgetThreadTailP = &FtpgetThread;

static char ftpASCII[] = "A";
static char ftpBinary[] = "I";

typedef struct _Ftpdata {
    StoreEntry *entry;
    char type_id;
    char host[SQUIDHOSTNAMELEN + 1];
    char request[MAX_URL];
    char user[MAX_URL];
    char password[MAX_URL];
    char *type;
    char *mime_hdr;
    int cpid;
    int ftp_fd;
    char *icp_page_ptr;		/* Used to send proxy-http request: 
				 * put_free_8k_page(me) if the lifetime
				 * expires */
    char *icp_rwd_ptr;		/* When a lifetime expires during the
				 * middle of an icpwrite, don't lose the
				 * icpReadWriteData */
} FtpData;

extern char *tmp_error_buf;
extern time_t cached_curtime;

/* XXX: this does not support FTP on a different port! */
int ftp_url_parser(url, data)
     char *url;
     FtpData *data;
{
    static char atypebuf[MAX_URL];
    static char hostbuf[MAX_URL];
    char *tmp = NULL;
    int t;
    char *host = data->host;
    char *request = data->request;
    char *user = data->user;
    char *password = data->password;

    /* initialize everything */
    atypebuf[0] = hostbuf[0] = '\0';
    request[0] = host[0] = user[0] = password[0] = '\0';

    t = sscanf(url, "%[a-zA-Z]://%[^/]%s", atypebuf, hostbuf, request);
    if ((t < 2) ||
	!(!strcasecmp(atypebuf, "ftp") || !strcasecmp(atypebuf, "file"))) {
	return -1;
    } else if (t == 2) {	/* no request */
	strcpy(request, "/");
    } else {
	tmp = url_convert_hex(request);		/* convert %xx to char */
	strncpy(request, tmp, MAX_URL);
	safe_free(tmp);
    }

    /* url address format is something like this:
     * [ userid [ : password ] @ ] host 
     * or possibly even
     * [ [ userid ] [ : [ password ] ] @ ] host
     * 
     * So we must try to make sense of it.  */

    /* XXX: this only support [user:passwd@]host */
    t = sscanf(hostbuf, "%[^:]:%[^@]@%s", user, password, host);
    if (t < 3) {
	strcpy(host, user);	/* no login/passwd information */
	strcpy(user, "anonymous");
	strcpy(password, "harvest@");
    }
    /* we need to convert user and password for URL encodings */
    tmp = url_convert_hex(user);
    strcpy(user, tmp);
    safe_free(tmp);

    tmp = url_convert_hex(password);
    strcpy(password, tmp);
    safe_free(tmp);

    return 0;
}

int ftpCachable(url, type, mime_hdr)
     char *url;
     char *type;
     char *mime_hdr;
{
    stoplist *p = NULL;

    /* scan stop list */
    p = ftp_stoplist;
    while (p) {
	if (strstr(url, p->key))
	    return 0;
	p = p->next;
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
    debug(4, "ftpLifeTimeExpire: FD %d: <URL:%s>\n", fd, entry->url);
    if (data->icp_page_ptr) {
	put_free_8k_page(data->icp_page_ptr);
	data->icp_page_ptr = NULL;
    }
    safe_free(data->icp_rwd_ptr);
    cached_error(entry, ERR_LIFETIME_EXP);
    /* ftp_close_pipe(data->ftp_fd, data->cpid); */
    comm_close(fd);
    safe_free(data);
}



/* This will be called when data is ready to be read from fd.  Read until
 * error or connection closed. */
int ftpReadReply(fd, data)
     int fd;
     FtpData *data;
{
    static char buf[4096];
    int len;
    int clen;
    int off;
    StoreEntry *entry = NULL;

    entry = data->entry;
    if (entry->flag & DELETE_BEHIND) {
	if (storeClientWaiting(entry)) {
	    /* check if we want to defer reading */
	    clen = entry->mem_obj->e_current_len;
	    off = entry->mem_obj->e_lowest_offset;
	    if ((clen - off) > FTP_DELETE_GAP) {
		debug(3, "ftpReadReply: Read deferred for Object: %s\n",
		    entry->key);
		debug(3, "                Current Gap: %d bytes\n",
		    clen - off);

		/* reschedule, so it will automatically be reactivated when
		 * Gap is big enough. */
		comm_set_select_handler(fd,
		    COMM_SELECT_READ,
		    (PF) ftpReadReply,
		    (caddr_t) data);
		return 0;
	    }
	} else {
	    /* we can terminate connection right now */
	    cached_error(entry, ERR_NO_CLIENTS_BIG_OBJ);
	    /* ftp_close_pipe(data->ftp_fd, data->cpid); */
	    comm_close(fd);
	    safe_free(data);
	    return 0;
	}
    }
    len = read(fd, buf, 4096);
    debug(5, "ftpReadReply FD %d, Read %d bytes\n", fd, len);

    if (len < 0 || ((len == 0) && (entry->mem_obj->e_current_len == 0))) {
	if (len < 0)
	    debug(1, "ftpReadReply - error reading: %s\n", xstrerror());
	cached_error(entry, ERR_READ_ERROR);
	/* ftp_close_pipe(data->ftp_fd, data->cpid); */
	comm_close(fd);
	safe_free(data);
    } else if (len == 0) {
	/* Connection closed; retrieval done. */
	/* If ftpget failed, arrange so the object gets ejected and
	 * doesn't get to disk. */
	/* XXX REALLY NEED TO THINK ABOUT THIS */
#ifdef OLD_CODE
	if (ftp_close_pipe(data->ftp_fd, data->cpid) != 0) {
	    entry->expires = cached_curtime + getNegativeTTL();
	    BIT_RESET(entry->flag, CACHABLE);
	    BIT_SET(entry->flag, RELEASE_REQUEST);
	} else
#endif
	if (!(entry->flag & DELETE_BEHIND)) {
	    entry->expires = cached_curtime + ttlSet(entry);
	}
	/* update fdstat and fdtable */
	comm_close(fd);
	storeComplete(entry);
	safe_free(data);
    } else if (((entry->mem_obj->e_current_len + len) > getFtpMax()) &&
	!(entry->flag & DELETE_BEHIND)) {
	/*  accept data, but start to delete behind it */
	storeStartDeleteBehind(entry);

	storeAppend(entry, buf, len);
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) ftpReadReply,
	    (caddr_t) data);

    } else if (entry->flag & CLIENT_ABORT_REQUEST) {
	/* append the last bit of info we get */
	storeAppend(entry, buf, len);
	cached_error(entry, ERR_CLIENT_ABORT);
	/* ftp_close_pipe(data->ftp_fd, data->cpid); */
	comm_close(fd);
	safe_free(data);
    } else {
	storeAppend(entry, buf, len);
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) ftpReadReply,
	    (caddr_t) data);
	comm_set_select_handler_plus_timeout(fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) ftpLifetimeExpire,
	    (caddr_t) data,
	    getReadTimeout());
    }
    return 0;
}

void ftpSendComplete(fd, buf, size, errflag, data)
     int fd;
     char *buf;
     int size;
     int errflag;
     FtpData *data;
{
    StoreEntry *entry = NULL;

    entry = data->entry;
    debug(5, "ftpSendComplete: FD %d: size %d: errflag %d.\n",
	fd, size, errflag);

    if (buf) {
	put_free_8k_page(buf);	/* Allocated by ftpSendRequest. */
	buf = NULL;
    }
    data->icp_page_ptr = NULL;	/* So lifetime expire doesn't re-free */
    data->icp_rwd_ptr = NULL;	/* Don't double free in lifetimeexpire */

    if (errflag) {
	cached_error(entry, ERR_CONNECT_FAIL, xstrerror());
	comm_close(fd);
	safe_free(data);
	return;
    } else {
	comm_set_select_handler(data->ftp_fd,
	    COMM_SELECT_READ,
	    (PF) ftpReadReply,
	    (caddr_t) data);
	comm_set_select_handler_plus_timeout(data->ftp_fd,
	    COMM_SELECT_TIMEOUT,
	    (PF) ftpLifetimeExpire,
	    (caddr_t) data, getReadTimeout());
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
    static char *w_space = " \t\r\n";
    char *s = NULL;
    int got_timeout = 0;
    int got_negttl = 0;
    int buflen;

    debug(5, "ftpSendRequest: FD %d\n", fd);

    buflen = strlen(data->request) + 256;
    buf = (char *) get_free_8k_page();
    data->icp_page_ptr = buf;
    memset(buf, '\0', buflen);

    path = data->request;
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

    /* Remove leading slash from FTP url-path so that we can
     *  handle ftp://user:pw@host/path objects where path and /path
     *  are quite different.         -DW */
    if (!strcmp(path, "/"))
	*path = '.';
    if (*path == '/')
	path++;

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
    strcat(buf, "-h ");		/* httpify */
    strcat(buf, "- ");		/* stdout */
    strcat(buf, data->host);
    strcat(buf, space);
    strcat(buf, path);
    strcat(buf, space);
    strcat(buf, mode);		/* A or I */
    strcat(buf, space);
    strcat(buf, data->user);
    strcat(buf, space);
    strcat(buf, data->password);
    strcat(buf, space);
    debug(5, "ftpSendRequest: FD %d: buf '%s'\n", fd, buf);
    data->icp_rwd_ptr = icpWrite(fd, buf, strlen(buf), 30, ftpSendComplete, data);
}

void ftpConnInProgress(fd, data)
     int fd;
     FtpData *data;
{
    StoreEntry *entry = data->entry;

    debug(5, "ftpConnInProgress: FD %d\n", fd);

    if (comm_connect(fd, "localhost", 3131) != COMM_OK)
	switch (errno) {
	case EINPROGRESS:
	case EALREADY:
	    /* schedule this handler again */
	    comm_set_select_handler(fd,
		COMM_SELECT_WRITE,
		(PF) ftpConnInProgress,
		(caddr_t) data);
	    return;
	case EISCONN:
	    debug(5, "ftpConnInProgress: FD %d is now connected.", fd);
	    break;		/* cool, we're connected */
	default:
	    comm_close(fd);
	    cached_error(entry, ERR_CONNECT_FAIL, xstrerror());
	    safe_free(data);
	    return;
	}
    /* Call the real write handler, now that we're fully connected */
    comm_set_select_handler(fd,
	COMM_SELECT_WRITE,
	(PF) ftpSendRequest,
	(caddr_t) data);
}



int ftpStart(unusedfd, url, entry)
     int unusedfd;
     char *url;
     StoreEntry *entry;
{
    FtpData *data = NULL;
    int status;

    debug(3, "FtpStart: FD %d <URL:%s>\n", unusedfd, url);

    data = (FtpData *) xcalloc(1, sizeof(FtpData));
    data->entry = entry;

    /* Parse url. */
    if (ftp_url_parser(url, data)) {
	cached_error(entry, ERR_INVALID_URL);
	safe_free(data);
	return COMM_ERROR;
    }
    debug(5, "FtpStart: FD %d, host=%s, request=%s, user=%s, passwd=%s\n",
	unusedfd, data->host, data->request, data->user, data->password);

    data->ftp_fd = comm_open(COMM_NONBLOCKING, 0, 0, url);
    if (data->ftp_fd == COMM_ERROR) {
	cached_error(entry, ERR_CONNECT_FAIL, xstrerror());
	safe_free(data);
	return COMM_ERROR;
    }
    /* Pipe/socket created ok */

    /* Now connect ... */
    if ((status = comm_connect(data->ftp_fd, "localhost", 3131))) {
	if (status != EINPROGRESS) {
	    comm_close(data->ftp_fd);
	    cached_error(entry, ERR_CONNECT_FAIL, xstrerror());
	    safe_free(data);
	    return COMM_ERROR;
	} else {
	    debug(5, "ftpStart: FD %d: EINPROGRESS.\n", data->ftp_fd);
	    comm_set_select_handler(data->ftp_fd, COMM_SELECT_LIFETIME,
		(PF) ftpLifetimeExpire, (caddr_t) data);
	    comm_set_select_handler(data->ftp_fd, COMM_SELECT_WRITE,
		(PF) ftpConnInProgress, (caddr_t) data);
	    return COMM_OK;
	}
    }
    fdstat_open(data->ftp_fd, Socket);
    commSetNonBlocking(data->ftp_fd);
    (void) fd_note(data->ftp_fd, entry->url);

    /* Install connection complete handler. */
    fd_note(data->ftp_fd, entry->url);
    comm_set_select_handler(data->ftp_fd,
	COMM_SELECT_WRITE,
	(PF) ftpSendRequest,
	(caddr_t) data);
    comm_set_fd_lifetime(data->ftp_fd,
	getClientLifetime());
    comm_set_select_handler(data->ftp_fd,
	COMM_SELECT_LIFETIME,
	(PF) ftpLifetimeExpire,
	(caddr_t) data);

    return COMM_OK;
}

int ftpInitialize()
{
    int pid;
    int fd;
    int p[2];
    static char pbuf[128];
    char *ftpget = getFtpProgram();

    if (pipe(p) < 0) {
	debug(0, "ftpInitialize: pipe: %s\n", xstrerror());
	return -1;
    }
    if ((pid = fork()) < 0) {
	debug(0, "ftpInitialize: fork: %s\n", xstrerror());
	return -1;
    }
    if (pid != 0) {		/* parent */
	close(p[0]);
	fdstat_open(p[1], Pipe);
	fd_note(p[1], "ftpget -S");
	fcntl(p[1], F_SETFD, 1);	/* set close-on-exec */
	return 0;
    }
    /* child */
    close(0);
    dup(p[0]);
    close(p[0]);
    close(p[1]);
    /* inherit stdin,stdout,stderr */
    for (fd = 3; fd < fdstat_biggest_fd(); fd++)
	(void) close(fd);
    sprintf(pbuf, "%d", 3131);
    execlp(ftpget, ftpget, "-D26,1", "-S", pbuf, NULL);
    debug(0, "ftpInitialize: %s: %s\n", ftpget, xstrerror());
    _exit(1);
    return (1);			/* eliminate compiler warning */
}

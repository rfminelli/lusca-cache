
/*
 * $Id$
 *
 * DEBUG: section 4     Error Generation
 * AUTHOR: Duane Wessels
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * --------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by
 *  the National Science Foundation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  
 */

#include "squid.h"

const char *err_string[] =
{
    "ERR_NONE",
    "ERR_READ_TIMEOUT",
    "ERR_LIFETIME_EXP",
    "ERR_NO_CLIENTS",
    "ERR_READ_ERROR",
    "ERR_WRITE_ERROR",
    "ERR_CLIENT_ABORT",
    "ERR_CONNECT_FAIL",
    "ERR_INVALID_REQ",
    "ERR_UNSUP_REQ",
    "ERR_INVALID_URL",
    "ERR_SOCKET_FAILURE",
    "ERR_DNS_FAIL",
    "ERR_NOT_IMPLEMENTED",
    "ERR_CANNOT_FORWARD",
    "ERR_NO_RELAY",
    "ERR_DISK_IO",
    "ERR_ZERO_SIZE_OBJECT",
    "ERR_FTP_DISABLED",
    "ERR_PROXY_DENIED",
    "ERR_CACHE_MISS_DENIED",
    "ERR_ACCESS_DENIED",
    "ERR_MAX"
};

static char *error_text[ERR_MAX];

static void errorStateFree _PARAMS((ErrorState * err));
static char *errorConvert _PARAMS((char token, ErrorState * err));
static char *errorBuildBuf _PARAMS((ErrorState * err, int *len));
static CWCB errorSendComplete;

void
errorInitialize(void)
{
    err_type i;
    int fd;
    char path[MAXPATHLEN];
    struct stat sb;
    assert(sizeof(err_string) == (ERR_MAX + 1) * 4);
    for (i = ERR_NONE + 1; i < ERR_MAX; i++) {
	snprintf(path, MAXPATHLEN, "%s/%s",
	    Config.errorDirectory, err_string[i]);
	fd = file_open(path, O_RDONLY, NULL, NULL);
	if (fd < 0) {
	    debug(4, 0) ("errorInitialize: %s: %s\n", path, xstrerror());
	    fatal("Failed to open error text file");
	}
	if (fstat(fd, &sb) < 0)
	    fatal_dump("stat() failed on error text file");
	safe_free(error_text[i]);
	error_text[i] = xcalloc(sb.st_size, 1);
	if (read(fd, error_text[i], sb.st_size) != sb.st_size)
	    fatal_dump("failed to fully read error text file");
	file_close(fd);
    }
}

static void
errorStateFree(ErrorState * err)
{
    requestUnlink(err->request);
    safe_free(err->redirect_url);
    safe_free(err->url);
    cbdataFree(err);
}

#define CVT_BUF_SZ 512
static char *
errorConvert(char token, ErrorState * err)
{
    char *p = NULL;
    request_t *r = err->request;
    static char buf[CVT_BUF_SZ];
    switch (token) {
    case 'U':
	p = r ? urlCanonicalClean(r) : err->url;
	break;
    case 'H':
	p = r ? r->host : "[unknown host]";
	break;
    case 'p':
	if (r) {
	    snprintf(buf, CVT_BUF_SZ, "%d", (int) r->port);
	    p = buf;
	} else {
	    p = "[unknown port]";
	}
	break;
    case 'P':
	p = r ? (char *) ProtocolStr[r->protocol] : "[unkown protocol]";
	break;
    case 'M':
	p = r ? (char *) RequestMethodStr[r->method] : "[unkown method]";
	break;
    case 'z':
	p = err->dnsserver_msg;
	break;
    case 'e':
	snprintf(buf, CVT_BUF_SZ, "%d", err->errno);
	p=buf;
	break;
    case 'E':
	snprintf(buf, CVT_BUF_SZ, "(%d) %s", err->errno, xstrerror());
	break;
/*
 * e - errno            			x
 * E - strerror()				x
 * t - local time
 * T - UTC
 * c - Squid error code
 * I - server IP address
 * i - client IP address
 * L - HREF link for more info/contact
 * w - cachemgr email address
 * h - cache hostname
 * d - seconds elapsed since request received
 * p - URL port #
 */
    default:
	p = "%UNKNOWN%";
	break;
    }
    if (p == NULL)
	p = "<NULL>";
    debug(4, 3) ("errorConvert: %%%c --> '%s'\n", token, p);
    return p;
}

#define ERROR_BUF_SZ (MAX_URL<<2)

static char *
errorBuildBuf(ErrorState * err, int *len)
{
    LOCAL_ARRAY(char, buf, ERROR_BUF_SZ);
    LOCAL_ARRAY(char, content, ERROR_BUF_SZ);
    char *hdr;
    int clen;
    int tlen;
    char *m;
    char *mx;
    char *p;
    char *t;
    assert(err != NULL);
    assert(err->type > ERR_NONE && err->type < ERR_MAX);
    mx = m = xstrdup(error_text[err->type]);
    clen = 0;
    while ((p = strchr(m, '%'))) {
	*p = '\0';		/* terminate */
	xstrncpy(content + clen, m, ERROR_BUF_SZ - clen);	/* copy */
	clen += (p - m);	/* advance */
	if (clen >= ERROR_BUF_SZ)
	    break;
	p++;
	m = p + 1;
	t = errorConvert(*p, err);	/* convert */
	xstrncpy(content + clen, t, ERROR_BUF_SZ - clen);	/* copy */
	clen += strlen(t);	/* advance */
	if (clen >= ERROR_BUF_SZ)
	    break;
    }
    if (clen < ERROR_BUF_SZ && m != NULL) {
	xstrncpy(content + clen, m, ERROR_BUF_SZ - clen);
	clen += strlen(m);
    }
    if (clen >= ERROR_BUF_SZ) {
	clen = ERROR_BUF_SZ - 1;
	*(content + clen) = '\0';
    }
    assert(clen == strlen(content));
    hdr = httpReplyHeader((double) 1.0,
	err->http_status,
	"text/html",
	clen,
	0,			/* no LMT for error pages */
	squid_curtime);
    tlen = snprintf(buf, ERROR_BUF_SZ, "%s\r\n%s", hdr, content);
    if (len)
	*len = tlen;
    xfree(mx);
    return buf;
}

void
errorSend(int fd, ErrorState * err)
{
    char *buf;
    int len;
    assert(fd >= 0);
    buf = errorBuildBuf(err, &len);
    cbdataAdd(err);
    cbdataLock(err);
    comm_write(fd, xstrdup(buf), len, errorSendComplete, err, xfree);
}

void
errorAppendEntry(StoreEntry * entry, ErrorState * err)
{
    char *buf;
    MemObject *mem = entry->mem_obj;
    int len;
    assert(entry->store_status == STORE_PENDING);
    buf = errorBuildBuf(err, &len);
    storeAppend(entry, buf, len);
    if (mem)
	mem->reply->code = err->http_status;
}

static void
errorSendComplete(int fd, char *buf, int size, int errflag, void *data)
{
    ErrorState *err = data;
    if (err->callback)
	err->callback(fd, err->callback_data, size);
    cbdataUnlock(err);
    errorStateFree(err);
}


/*
 * $Id$
 *
 * DEBUG: section 12    Client Handling
 * AUTHOR: Harvest Derived
 *
 * SQUID Internet Object Cache  http://www.nlanr.net/Squid/
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

/*
 * Copyright (c) 1994, 1995.  All rights reserved.
 *  
 *   The Harvest software was developed by the Internet Research Task
 *   Force Research Group on Resource Discovery (IRTF-RD):
 *  
 *         Mic Bowman of Transarc Corporation.
 *         Peter Danzig of the University of Southern California.
 *         Darren R. Hardy of the University of Colorado at Boulder.
 *         Udi Manber of the University of Arizona.
 *         Michael F. Schwartz of the University of Colorado at Boulder.
 *         Duane Wessels of the University of Colorado at Boulder.
 *  
 *   This copyright notice applies to software in the Harvest
 *   ``src/'' directory only.  Users should consult the individual
 *   copyright notices in the ``components/'' subdirectories for
 *   copyright information about other software bundled with the
 *   Harvest source code distribution.
 *  
 * TERMS OF USE
 *   
 *   The Harvest software may be used and re-distributed without
 *   charge, provided that the software origin and research team are
 *   cited in any use of the system.  Most commonly this is
 *   accomplished by including a link to the Harvest Home Page
 *   (http://harvest.cs.colorado.edu/) from the query page of any
 *   Broker you deploy, as well as in the query result pages.  These
 *   links are generated automatically by the standard Broker
 *   software distribution.
 *   
 *   The Harvest software is provided ``as is'', without express or
 *   implied warranty, and with no support nor obligation to assist
 *   in its use, correction, modification or enhancement.  We assume
 *   no liability with respect to the infringement of copyrights,
 *   trade secrets, or any patents, and are not responsible for
 *   consequential damages.  Proper use of the Harvest software is
 *   entirely the responsibility of the user.
 *  
 * DERIVATIVE WORKS
 *  
 *   Users may make derivative works from the Harvest software, subject 
 *   to the following constraints:
 *  
 *     - You must include the above copyright notice and these 
 *       accompanying paragraphs in all forms of derivative works, 
 *       and any documentation and other materials related to such 
 *       distribution and use acknowledge that the software was 
 *       developed at the above institutions.
 *  
 *     - You must notify IRTF-RD regarding your distribution of 
 *       the derivative work.
 *  
 *     - You must clearly notify users that your are distributing 
 *       a modified version and not the original Harvest software.
 *  
 *     - Any derivative product is also subject to these copyright 
 *       and use restrictions.
 *  
 *   Note that the Harvest software is NOT in the public domain.  We
 *   retain copyright, as specified above.
 *  
 * HISTORY OF FREE SOFTWARE STATUS
 *  
 *   Originally we required sites to license the software in cases
 *   where they were going to build commercial products/services
 *   around Harvest.  In June 1995 we changed this policy.  We now
 *   allow people to use the core Harvest software (the code found in
 *   the Harvest ``src/'' directory) for free.  We made this change
 *   in the interest of encouraging the widest possible deployment of
 *   the technology.  The Harvest software is really a reference
 *   implementation of a set of protocols and formats, some of which
 *   we intend to standardize.  We encourage commercial
 *   re-implementations of code complying to this set of standards.  
 */

#include "squid.h"

int neighbors_do_private_keys = 1;

static char *log_tags[] =
{
    "LOG_NONE",
    "TCP_HIT",
    "TCP_MISS",
    "TCP_EXPIRED_HIT",
    "TCP_EXPIRED_MISS",
    "TCP_REFRESH",
    "TCP_IMS_HIT",
    "TCP_IMS_MISS",
    "TCP_SWAPFAIL",
    "TCP_DENIED",
    "UDP_HIT",
    "UDP_HIT_OBJ",
    "UDP_MISS",
    "UDP_DENIED",
    "UDP_INVALID",
    "UDP_RELOADING",
    "ERR_READ_TIMEOUT",
    "ERR_LIFETIME_EXP",
    "ERR_NO_CLIENTS_BIG_OBJ",
    "ERR_READ_ERROR",
    "ERR_CLIENT_ABORT",
    "ERR_CONNECT_FAIL",
    "ERR_INVALID_REQ",
    "ERR_UNSUP_REQ",
    "ERR_INVALID_URL",
    "ERR_NO_FDS",
    "ERR_DNS_FAIL",
    "ERR_NOT_IMPLEMENTED",
    "ERR_CANNOT_FETCH",
    "ERR_NO_RELAY",
    "ERR_DISK_IO",
    "ERR_ZERO_SIZE_OBJECT",
    "ERR_PROXY_DENIED"
};

static icpUdpData *UdpQueueHead = NULL;
static icpUdpData *UdpQueueTail = NULL;
#define ICP_SENDMOREDATA_BUF SM_PAGE_SIZE

typedef struct {
    int fd;
    struct sockaddr_in to;
    StoreEntry *entry;
    icp_common_t header;
    struct timeval started;
} icpHitObjStateData;

/* Local functions */
static void icpHandleStoreComplete __P((int, char *, int, int, void *icpState));
static void icpHandleStoreIMS __P((int, StoreEntry *, icpStateData *));
static void icpHandleIMSComplete __P((int, char *, int, int, void *icpState));
static int icpProcessMISS __P((int, icpStateData *));
static void CheckQuickAbort __P((icpStateData *));
static int CheckQuickAbort2 __P((icpStateData *));
extern void identStart __P((int, icpStateData *));
static void icpHitObjHandler __P((int, void *));
static void icpLogIcp __P((icpUdpData *));
static void icpHandleIcpV2 __P((int fd, struct sockaddr_in, char *, int len));
static void icpHandleIcpV3 __P((int fd, struct sockaddr_in, char *, int len));
static char *icpConstruct304reply __P((struct _http_reply *));
static void icpUdpSendEntry(int fd,
    char *url,
    icp_common_t * reqheaderp,
    struct sockaddr_in *to,
    icp_opcode opcode,
    StoreEntry * entry,
    struct timeval start_time);


/* This is a handler normally called by comm_close() */
static int
icpStateFree(int fd, icpStateData * icpState)
{
    int http_code = 0;
    int elapsed_msec;
    struct _hierarchyLogData *hierData = NULL;

    if (!icpState)
	return 1;
    if (icpState->log_type < LOG_TAG_NONE || icpState->log_type > ERR_MAX)
	fatal_dump("icpStateFree: icpState->log_type out of range.");
    if (icpState->entry) {
	if (icpState->entry->mem_obj)
	    http_code = icpState->entry->mem_obj->reply->code;
    } else {
	http_code = icpState->http_code;
    }
    elapsed_msec = tvSubMsec(icpState->start, current_time);
    if (icpState->request)
	hierData = &icpState->request->hierarchy;
    HTTPCacheInfo->log_append(HTTPCacheInfo,
	icpState->url,
	icpState->log_addr,
	icpState->size,
	log_tags[icpState->log_type],
	RequestMethodStr[icpState->method],
	http_code,
	elapsed_msec,
	icpState->ident,
#if !LOG_FULL_HEADERS
	hierData);
#else
	hierData,
	icpState->request_hdr,
	icpState->reply_hdr);
#endif /* LOG_FULL_HEADERS */
    HTTPCacheInfo->proto_count(HTTPCacheInfo,
	icpState->request ? icpState->request->protocol : PROTO_NONE,
	icpState->log_type);
    if (icpState->ident_fd)
	comm_close(icpState->ident_fd);
    safe_free(icpState->inbuf);
    meta_data.misc -= icpState->inbufsize;
    safe_free(icpState->url);
    safe_free(icpState->request_hdr);
#if LOG_FULL_HEADERS
    safe_free(icpState->reply_hdr);
#endif /* LOG_FULL_HEADERS */
    if (icpState->entry) {
	storeUnregister(icpState->entry, fd);
	storeUnlockObject(icpState->entry);
	icpState->entry = NULL;
    }
    requestUnlink(icpState->request);
    if (icpState->aclChecklist) {
	debug(12, 0, "icpStateFree: still have aclChecklist!\n");
	requestUnlink(icpState->aclChecklist->request);
	safe_free(icpState->aclChecklist);
    }
    safe_free(icpState);
    return 0;			/* XXX gack, all comm handlers return ints */
}

void
icpParseRequestHeaders(icpStateData * icpState)
{
    char *request_hdr = icpState->request_hdr;
    char *t = NULL;
    if (mime_get_header(request_hdr, "If-Modified-Since"))
	BIT_SET(icpState->flags, REQ_IMS);
    if ((t = mime_get_header(request_hdr, "Pragma"))) {
	if (!strcasecmp(t, "no-cache"))
	    BIT_SET(icpState->flags, REQ_NOCACHE);
    }
    if (mime_get_header(request_hdr, "Authorization"))
	BIT_SET(icpState->flags, REQ_AUTH);
    if (strstr(request_hdr, ForwardedBy))
	BIT_SET(icpState->flags, REQ_LOOPDETECT);
}

static int
icpCachable(icpStateData * icpState)
{
    char *request = icpState->url;
    request_t *req = icpState->request;
    method_t method = req->method;
    if (BIT_TEST(icpState->flags, REQ_AUTH))
	return 0;
    if (req->protocol == PROTO_HTTP)
	return httpCachable(request, method);
    /* FTP is always cachable */
    if (req->protocol == PROTO_GOPHER)
	return gopherCachable(request);
    if (req->protocol == PROTO_WAIS)
	return 0;
    if (method == METHOD_CONNECT)
	return 0;
    if (req->protocol == PROTO_CACHEOBJ)
	return 0;
    return 1;
}

/* Return true if we can query our neighbors for this object */
static int
icpHierarchical(icpStateData * icpState)
{
    char *request = icpState->url;
    request_t *req = icpState->request;
    method_t method = req->method;
    wordlist *p = NULL;
    /* IMS needs a private key, so we can use the hierarchy for IMS only
     * if our neighbors support private keys */
    if (BIT_TEST(icpState->flags, REQ_IMS) && !neighbors_do_private_keys)
	return 0;
    if (BIT_TEST(icpState->flags, REQ_AUTH))
	return 0;
    if (method != METHOD_GET)
	return 0;
    /* scan hierarchy_stoplist */
    for (p = Config.hierarchy_stoplist; p; p = p->next)
	if (strstr(request, p->key))
	    return 0;
    if (BIT_TEST(icpState->flags, REQ_LOOPDETECT))
	return 0;
    if (req->protocol == PROTO_HTTP)
	return httpCachable(request, method);
    if (req->protocol == PROTO_GOPHER)
	return gopherCachable(request);
    if (req->protocol == PROTO_WAIS)
	return 0;
    if (req->protocol == PROTO_CACHEOBJ)
	return 0;
    return 1;
}

void
icpSendERRORComplete(int fd, char *buf, int size, int errflag, void *data)
{
    icpStateData *icpState = data;
    debug(12, 4, "icpSendERRORComplete: FD %d: sz %d: err %d.\n",
	fd, size, errflag);
    icpState->size += size;
    comm_close(fd);
}

/* Send ERROR message. */
int
icpSendERROR(int fd, log_type errorCode, char *text, icpStateData * icpState, int httpCode)
{
    int buf_len = 0;
    u_short port = 0;
    char *buf = NULL;

    port = comm_local_port(fd);
    debug(12, 4, "icpSendERROR: code %d: port %hd: text: '%s'\n",
	errorCode, port, text);

    if (port == 0) {
	/* This file descriptor isn't bound to a socket anymore.
	 * It probably timed out. */
	debug(12, 2, "icpSendERROR: COMM_ERROR text: %80.80s\n", text);
	comm_close(fd);
	return COMM_ERROR;
    }
    if (port != Config.Port.http) {
	sprintf(tmp_error_buf, "icpSendERROR: FD %d unexpected port %hd.",
	    fd, port);
	fatal_dump(tmp_error_buf);
    }
    icpState->log_type = errorCode;
    icpState->http_code = httpCode;
    /* Error message for the ascii port */
    buf_len = strlen(text);
    buf_len = buf_len > 4095 ? 4095 : buf_len;
    buf = get_free_4k_page();
    strncpy(buf, text, buf_len);
    *(buf + buf_len) = '\0';
    comm_write(fd,
	buf,
	buf_len,
	30,
	icpSendERRORComplete,
	(void *) icpState,
	put_free_4k_page);
    return COMM_OK;
}

#if LOG_FULL_HEADERS
/*
 * Scan beginning of swap object being returned and hope that it contains
 * a complete MIME header for use in reply header logging (log_append).
 */
void
icp_maybe_remember_reply_hdr(icpStateData * icpState)
{
    char *mime;
    char *end;

    if (icpState && icpState->entry && icpState->entry->mem_obj
	&& icpState->entry->mem_obj->data && icpState->entry->mem_obj->data->head
	&& (mime = icpState->entry->mem_obj->data->head->data) != NULL
	&& (end = mime_headers_end(mime)) != NULL) {
	int mime_len = end - mime;
	char *buf = xcalloc(mime_len + 1, 1);

	strncpy(buf, mime, mime_len);
	buf[mime_len] = 0;
	icpState->reply_hdr = buf;
	debug(12, 5, "icp_maybe_remember_reply_hdr: ->\n%s<-\n", buf);
    } else {
	icpState->reply_hdr = 0;
    }
}

#endif /* LOG_FULL_HEADERS */
/* Send available data from an object in the cache.  This is called either
 * on select for  write or directly by icpHandleStore. */

int
icpSendMoreData(int fd, icpStateData * icpState)
{
    StoreEntry *entry = icpState->entry;
    int len;
    int tcode = 555;
    double http_ver;
    LOCAL_ARRAY(char, scanbuf, 20);
    char *buf = NULL;
    char *p = NULL;

    debug(12, 5, "icpSendMoreData: <URL:%s> sz %d: len %d: off %d.\n",
	entry->url,
	entry->object_len,
	entry->mem_obj ? entry->mem_obj->e_current_len : 0,
	icpState->offset);

    buf = get_free_4k_page();
    storeClientCopy(icpState->entry, icpState->offset, ICP_SENDMOREDATA_BUF, buf, &len, fd);

    /* look for HTTP reply code */
    if (icpState->offset == 0 && entry->mem_obj->reply->code == 0 && len > 0) {
	memset(scanbuf, '\0', 20);
	xmemcpy(scanbuf, buf, len > 19 ? 19 : len);
	sscanf(scanbuf, "HTTP/%lf %d", &http_ver, &tcode);
	entry->mem_obj->reply->code = tcode;
#if LOG_FULL_HEADERS
	icp_maybe_remember_reply_hdr(icpState);
#endif /* LOG_FULL_HEADERS */
    }
    icpState->offset += len;
    if (icpState->request->method == METHOD_HEAD && (p = mime_headers_end(buf))) {
	*p = '\0';
	len = p - buf;
	icpState->offset = entry->mem_obj->e_current_len;	/* force end */
    }
    comm_write(fd,
	buf,
	len,
	30,
	icpHandleStoreComplete,
	(void *) icpState,
	put_free_4k_page);
    return COMM_OK;
}

/* Called by storage manager when more data arrives from source. 
 * Starts state machine towards client with new batch of data or
 * error messages.  We get here by invoking the handlers in the
 * pending list.
 */
void
icpHandleStore(int fd, StoreEntry * entry, icpStateData * icpState)
{
    debug(12, 5, "icpHandleStore: FD %d: off %d: <URL:%s>\n",
	fd, icpState->offset, entry->url);
    if (entry->store_status == STORE_ABORTED) {
	debug(12, 3, "icpHandleStore: abort_code=%d\n",
	    entry->mem_obj->abort_code);
	icpSendERROR(fd,
	    entry->mem_obj->abort_code,
	    entry->mem_obj->e_abort_msg,
	    icpState,
	    400);
	return;
    }
    if (icpState->entry != entry)
	fatal_dump("icpHandleStore: entry mismatch!");
    icpSendMoreData(fd, icpState);
}

static void
icpHandleStoreComplete(int fd, char *buf, int size, int errflag, void *data)
{
    icpStateData *icpState = (icpStateData *) data;
    StoreEntry *entry = NULL;

    entry = icpState->entry;
    icpState->size += size;
    debug(12, 5, "icpHandleStoreComplete: FD %d: sz %d: err %d: off %d: len %d: tsmp %d: lref %d.\n",
	fd, size, errflag,
	icpState->offset, entry->object_len,
	entry->timestamp, entry->lastref);
    if (errflag) {
	/* if runs in quick abort mode, set flag to tell 
	 * fetching module to abort the fetching */
	CheckQuickAbort(icpState);
	/* Log the number of bytes that we managed to read */
	HTTPCacheInfo->proto_touchobject(HTTPCacheInfo,
	    urlParseProtocol(entry->url),
	    icpState->offset);
	/* Now we release the entry and DON'T touch it from here on out */
	comm_close(fd);
    } else if (icpState->offset < entry->mem_obj->e_current_len) {
	/* More data available locally; write it now */
	icpSendMoreData(fd, icpState);
    } else if (icpState->offset == entry->object_len &&
	entry->store_status != STORE_PENDING) {
	/* We're finished case */
	HTTPCacheInfo->proto_touchobject(HTTPCacheInfo,
	    icpState->request->protocol,
	    icpState->offset);
	comm_close(fd);
    } else {
	/* More data will be coming from primary server; register with 
	 * storage manager. */
	storeRegister(icpState->entry, fd, (PIF) icpHandleStore, (void *) icpState);
    }
}

static int
icpGetHeadersForIMS(int fd, icpStateData * icpState)
{
    StoreEntry *entry = icpState->entry;
    MemObject *mem = entry->mem_obj;
    int buf_len = icpState->offset;
    int len;
    int max_len = 8191 - buf_len;
    char *p = icpState->buf + buf_len;
    char *IMS_hdr = NULL;
    time_t IMS;
    int IMS_length;
    time_t date;
    int length;
    char *reply = NULL;

    if (max_len <= 0) {
	debug(12, 1, "icpGetHeadersForIMS: To much headers '%s'\n",
	    entry->key ? entry->key : entry->url);
	icpState->offset = 0;
	put_free_8k_page(icpState->buf);
	icpState->buf = NULL;
	return icpProcessMISS(fd, icpState);
    }
    storeClientCopy(entry, icpState->offset, max_len, p, &len, fd);
    buf_len = icpState->offset = +len;

    if (!mime_headers_end(icpState->buf)) {
	/* All headers are not yet available, wait for more data */
	storeRegister(entry, fd, (PIF) icpHandleStoreIMS, (void *) icpState);
	return COMM_OK;
    }
    /* All headers are available, check if object is modified or not */
    IMS_hdr = mime_get_header(icpState->request_hdr, "If-Modified-Since");
    httpParseHeaders(icpState->buf, mem->reply);

    if (!IMS_hdr)
	fatal_dump("icpGetHeadersForIMS: Cant find IMS header in request\n");

    /* Restart the object from the beginning */
    icpState->offset = 0;

    /* Only objects with statuscode==200 can be "Not modified" */
    /* XXX: Should we ignore this? */
    if (mem->reply->code != 200) {
	debug(12, 4, "icpGetHeadersForIMS: Reply code %d!=200\n",
	    mem->reply->code);
	put_free_8k_page(icpState->buf);
	icpState->buf = NULL;
	return icpProcessMISS(fd, icpState);
    }
    p = strtok(IMS_hdr, ";");
    IMS = parse_rfc850(p);
    IMS_length = -1;
    while ((p = strtok(NULL, ";"))) {
	while (isspace(*p))
	    p++;
	if (strncasecmp(p, "length=", 7) == 0)
	    IMS_length = atoi(strchr(p, '=') + 1);
    }

    /* Find date when the object last was modified */
    if (*mem->reply->last_modified)
	date = parse_rfc850(mem->reply->last_modified);
    else if (*mem->reply->date)
	date = parse_rfc850(mem->reply->date);
    else
	date = entry->timestamp;

    /* Find size of the object */
    if (mem->reply->content_length)
	length = mem->reply->content_length;
    else
	length = entry->object_len - mime_headers_size(icpState->buf);

    put_free_8k_page(icpState->buf);
    icpState->buf = NULL;
    icpState->log_type = LOG_TCP_IMS_HIT;

    /* Compare with If-Modified-Since header */
    if (IMS > date || (IMS == date && (IMS_length < 0 || IMS_length == length))) {
	/* The object is not modified */
	debug(12, 4, "icpGetHeadersForIMS: Not modified '%s'\n", entry->url);
	reply = icpConstruct304reply(mem->reply);
	comm_write(fd,
	    xstrdup(reply),
	    strlen(reply),
	    30,
	    icpHandleIMSComplete,
	    icpState,
	    xfree);
	return COMM_OK;
    } else {
	debug(12, 4, "icpGetHeadersForIMS: We have newer '%s'\n", entry->url);
	/* We have a newer object */
	return icpSendMoreData(fd, icpState);
    }
}

static void
icpHandleStoreIMS(int fd, StoreEntry * entry, icpStateData * icpState)
{
    icpGetHeadersForIMS(fd, icpState);
}

static void
icpHandleIMSComplete(int fd, char *buf_unused
    ,int size, int errflag, void *data)
{
    icpStateData *icpState = data;
    StoreEntry *entry = icpState->entry;
    debug(12, 5, "icpHandleIMSComplete: Not Modified sent '%s'\n", entry->url);
    HTTPCacheInfo->proto_touchobject(HTTPCacheInfo,
	icpState->request->protocol,
	size);
    /* Set up everything for the logging */
    storeUnlockObject(entry);
    icpState->entry = NULL;
    icpState->size += size;
    icpState->http_code = 304;
    comm_close(fd);
}

/*
 * Below, we check whether the object is a hit or a miss.  If it's a hit,
 * we check whether the object is still valid or whether it is a MISS_TTL.
 */
void
icpProcessRequest(int fd, icpStateData * icpState)
{
    char *url = icpState->url;
    char *pubkey = NULL;
    StoreEntry *entry = NULL;

    debug(12, 4, "icpProcessRequest: %s <URL:%s>\n",
	RequestMethodStr[icpState->method],
	url);
    if (icpState->method == METHOD_CONNECT) {
	icpState->log_type = LOG_TCP_MISS;
	sslStart(fd,
	    url,
	    icpState->request,
	    icpState->request_hdr,
	    &icpState->size);
	return;
    }
    if (icpCachable(icpState))
	BIT_SET(icpState->flags, REQ_CACHABLE);
    if (icpHierarchical(icpState))
	BIT_SET(icpState->flags, REQ_HIERARCHICAL);

    debug(12, 5, "icpProcessRequest: REQ_NOCACHE = %s\n",
	BIT_TEST(icpState->flags, REQ_NOCACHE) ? "SET" : "NOT SET");
    debug(12, 5, "icpProcessRequest: REQ_CACHABLE = %s\n",
	BIT_TEST(icpState->flags, REQ_CACHABLE) ? "SET" : "NOT SET");
    debug(12, 5, "icpProcessRequest: REQ_HIERARCHICAL = %s\n",
	BIT_TEST(icpState->flags, REQ_HIERARCHICAL) ? "SET" : "NOT SET");

    /* NOTE on HEAD requests: We currently don't cache HEAD reqeusts
     * at all, so look for the corresponding GET object, or just go
     * directly. The only way to get a TCP_HIT on a HEAD reqeust is
     * if someone already did a GET.  Maybe we should turn HEAD
     * misses into full GET's?  */
    if (icpState->method == METHOD_HEAD) {
	pubkey = storeGeneratePublicKey(icpState->url, METHOD_GET);
    } else
	pubkey = storeGeneratePublicKey(icpState->url, icpState->method);

    if ((entry = storeGet(pubkey)) == NULL) {
	/* This object isn't in the cache.  We do not hold a lock yet */
	icpState->log_type = LOG_TCP_MISS;
    } else if (BIT_TEST(icpState->flags, REQ_NOCACHE)) {
	/* IMS+NOCACHE should not eject valid object */
	if (!BIT_TEST(icpState->flags, REQ_IMS))
	    storeRelease(entry);
	ipcacheReleaseInvalid(icpState->request->host);
	entry = NULL;
	icpState->log_type = LOG_TCP_USER_REFRESH;
    } else if (!storeEntryValidToSend(entry)) {
	/* The object is in the cache, but is not valid.  Use
	 * LOG_TCP_EXPIRED_MISS for the time being, maybe change it to
	 * _HIT later in icpHandleIMSReply() */
	icpState->log_type = LOG_TCP_EXPIRED_MISS;
    } else if (BIT_TEST(icpState->flags, REQ_IMS)) {
	/* A cached IMS request */
	icpState->log_type = LOG_TCP_IMS_MISS;
    } else {
	icpState->log_type = LOG_TCP_HIT;
    }

    /* Lock the object */
    if (entry && storeLockObject(entry, NULL, NULL) < 0) {
	storeRelease(entry);
	entry = NULL;
	icpState->log_type = LOG_TCP_SWAPIN_FAIL;
    }
    icpState->entry = entry;	/* Save a reference to the object */
    icpState->offset = 0;

    debug(12, 4, "icpProcessRequest: %s for '%s'\n",
	log_tags[icpState->log_type],
	icpState->url);

    switch (icpState->log_type) {
    case LOG_TCP_HIT:
	icpSendMoreData(fd, icpState);
	break;
    case LOG_TCP_IMS_MISS:
	icpState->buf = get_free_8k_page();
	memset(icpState->buf, '\0', 8192);
	icpGetHeadersForIMS(fd, icpState);
	break;
    case LOG_TCP_EXPIRED_MISS:
	icpProcessExpired(fd, icpState);
	break;
    default:
	icpProcessMISS(fd, icpState);
	break;
    }
}


/*
 * Prepare to fetch the object as it's a cache miss of some kind.
 */
static int
icpProcessMISS(int fd, icpStateData * icpState)
{
    char *url = icpState->url;
    char *request_hdr = icpState->request_hdr;
    StoreEntry *entry = NULL;

    debug(12, 4, "icpProcessMISS: '%s %s'\n",
	RequestMethodStr[icpState->method], url);
    debug(12, 10, "icpProcessMISS: request_hdr:\n%s\n", request_hdr);

    /* Get rid of any references to a StoreEntry (if any) */
    if (icpState->entry) {
	storeUnregister(icpState->entry, fd);
	storeUnlockObject(icpState->entry);
	icpState->entry = NULL;
    }
    entry = storeCreateEntry(url,
	request_hdr,
	icpState->flags,
	icpState->method);
    /* NOTE, don't call storeLockObject(), storeCreateEntry() does it */

    entry->refcount++;		/* MISS CASE */
    entry->mem_obj->fd_of_first_client = fd;
    icpState->entry = entry;
    icpState->offset = 0;
    /* Register with storage manager to receive updates when data comes in. */
    storeRegister(entry, fd, (PIF) icpHandleStore, (void *) icpState);
    return (protoDispatch(fd, url, icpState->entry, icpState->request));
}

static void
icpLogIcp(icpUdpData * queue)
{
    icp_common_t *header = (icp_common_t *) (void *) queue->msg;
    char *url = (char *) header + sizeof(icp_common_t);
    HTTPCacheInfo->log_append(HTTPCacheInfo,
	url,
	queue->address.sin_addr,
	queue->len,
	log_tags[queue->logcode],
	IcpOpcodeStr[ICP_OP_QUERY],
	0,
	tvSubMsec(queue->start, current_time),
	NULL,			/* ident */
#if !LOG_FULL_HEADERS
	NULL);			/* hierarchy data */
#else
	NULL,			/* hierarchy data */
	NULL,			/* request header */
	NULL);			/* reply header */
#endif /* LOG_FULL_HEADERS */
    ICPCacheInfo->proto_touchobject(ICPCacheInfo,
	queue->proto,
	queue->len);
    ICPCacheInfo->proto_count(ICPCacheInfo,
	queue->proto,
	queue->logcode);
}

int
icpUdpReply(int fd, icpUdpData * queue)
{
    int result = COMM_OK;
    int x;
    /* Disable handler, in case of errors. */
    comm_set_select_handler(fd,
	COMM_SELECT_WRITE,
	0,
	0);
    while ((queue = UdpQueueHead)) {
	debug(12, 5, "icpUdpReply: FD %d sending %d bytes to %s port %d\n",
	    fd,
	    queue->len,
	    inet_ntoa(queue->address.sin_addr),
	    ntohs(queue->address.sin_port));
	x = comm_udp_sendto(fd,
	    &queue->address,
	    sizeof(struct sockaddr_in),
	    queue->msg,
	    queue->len);
	if (x < 0) {
	    if (errno == EWOULDBLOCK || errno == EAGAIN)
		break;		/* don't de-queue */
	    else
		result = COMM_ERROR;
	}
	UdpQueueHead = queue->next;
	if (queue->logcode)
	    icpLogIcp(queue);
	safe_free(queue->msg);
	safe_free(queue);
    }
    /* Reinstate handler if needed */
    if (UdpQueueHead) {
	comm_set_select_handler(fd,
	    COMM_SELECT_WRITE,
	    (PF) icpUdpReply,
	    (void *) UdpQueueHead);
    }
    return result;
}

int
icpUdpSend(int fd, char *url, icp_common_t * reqheaderp, struct sockaddr_in *to, int flags
    ,icp_opcode opcode, log_type logcode, protocol_t proto)
{
    char *buf = NULL;
    int buf_len = sizeof(icp_common_t) + strlen(url) + 1;
    icp_common_t *headerp = NULL;
    icpUdpData *data = xmalloc(sizeof(icpUdpData));
    struct sockaddr_in our_socket_name;
    int sock_name_length = sizeof(our_socket_name);
    char *urloffset = NULL;

#ifdef CHECK_BAD_ADDRS
    if (to->sin_addr.s_addr == 0xFFFFFFFF) {
	debug(12, 0, "icpUdpSend: URL '%s'\n", url);
	fatal_dump("icpUdpSend: BAD ADDRESS: 255.255.255.255");
    }
#endif

    if (getsockname(fd, (struct sockaddr *) &our_socket_name,
	    &sock_name_length) == -1) {
	debug(12, 1, "icpUdpSend: FD %d: getsockname failure: %s\n",
	    fd, xstrerror());
	return COMM_ERROR;
    }
    memset(data, '\0', sizeof(icpUdpData));
    data->address = *to;

    if (opcode == ICP_OP_QUERY)
	buf_len += sizeof(u_num32);
    buf = xcalloc(buf_len, 1);
    headerp = (icp_common_t *) (void *) buf;
    headerp->opcode = opcode;
    headerp->version = ICP_VERSION_CURRENT;
    headerp->length = htons(buf_len);
    headerp->reqnum = htonl(reqheaderp->reqnum);
    if (opcode == ICP_OP_QUERY && !BIT_TEST(flags, REFRESH_REQUEST))
	headerp->flags = htonl(ICP_FLAG_HIT_OBJ);
    headerp->pad = 0;
    /* xmemcpy(headerp->auth, , ICP_AUTH_SIZE); */
    headerp->shostid = htonl(our_socket_name.sin_addr.s_addr);
    debug(12, 5, "icpUdpSend: headerp->reqnum = %d\n", headerp->reqnum);

    urloffset = buf + sizeof(icp_common_t);

    if (opcode == ICP_OP_QUERY)
	urloffset += sizeof(u_num32);
    xmemcpy(urloffset, url, strlen(url));
    data->msg = buf;
    data->len = buf_len;
    data->start = current_time;
    data->logcode = logcode;
    data->proto = proto;

    debug(12, 4, "icpUdpSend: Queueing for %s: \"%s %s\"\n",
	inet_ntoa(to->sin_addr),
	IcpOpcodeStr[opcode],
	url);
    AppendUdp(data);
    comm_set_select_handler(fd,
	COMM_SELECT_WRITE,
	(PF) icpUdpReply,
	(void *) UdpQueueHead);
    return COMM_OK;
}

static void
icpUdpSendEntry(int fd, char *url, icp_common_t * reqheaderp, struct sockaddr_in *to, icp_opcode opcode, StoreEntry * entry, struct timeval start_time)
{
    char *buf = NULL;
    int buf_len;
    icp_common_t *headerp = NULL;
    icpUdpData *data = NULL;
    struct sockaddr_in our_socket_name;
    int sock_name_length = sizeof(our_socket_name);
    char *urloffset = NULL;
    char *entryoffset = NULL;
    MemObject *m = entry->mem_obj;
    u_short data_sz;
    int size;

    debug(12, 3, "icpUdpSendEntry: fd = %d\n", fd);
    debug(12, 3, "icpUdpSendEntry: url = '%s'\n", url);
    debug(12, 3, "icpUdpSendEntry: to = %s:%d\n", inet_ntoa(to->sin_addr), ntohs(to->sin_port));
    debug(12, 3, "icpUdpSendEntry: opcode = %d %s\n", opcode, IcpOpcodeStr[opcode]);
    debug(12, 3, "icpUdpSendEntry: entry = %p\n", entry);

    buf_len = sizeof(icp_common_t) + strlen(url) + 1 + 2 + entry->object_len;

#ifdef CHECK_BAD_ADDRS
    if (to->sin_addr.s_addr == 0xFFFFFFFF) {
	debug(12, 0, "icpUdpSendEntry: URL '%s'\n", url);
	fatal_dump("icpUdpSend: BAD ADDRESS: 255.255.255.255");
    }
#endif

    if (getsockname(fd, (struct sockaddr *) &our_socket_name,
	    &sock_name_length) == -1) {
	debug(12, 1, "icpUdpSendEntry: FD %d: getsockname failure: %s\n",
	    fd, xstrerror());
	return;
    }
    buf = xcalloc(buf_len, 1);
    headerp = (icp_common_t *) (void *) buf;
    headerp->opcode = opcode;
    headerp->version = ICP_VERSION_CURRENT;
    headerp->length = htons(buf_len);
    headerp->reqnum = htonl(reqheaderp->reqnum);
    headerp->flags = htonl(ICP_FLAG_HIT_OBJ);
    headerp->shostid = htonl(our_socket_name.sin_addr.s_addr);
    urloffset = buf + sizeof(icp_common_t);
    xmemcpy(urloffset, url, strlen(url));
    data_sz = htons((u_short) entry->object_len);
    entryoffset = urloffset + strlen(url) + 1;
    xmemcpy(entryoffset, &data_sz, sizeof(u_short));
    entryoffset += sizeof(u_short);
    size = m->data->mem_copy(m->data, 0, entryoffset, entry->object_len);
    if (size != entry->object_len) {
	debug(12, 1, "icpUdpSendEntry: copy failed, wanted %d got %d bytes\n",
	    entry->object_len, size);
	safe_free(buf);
	return;
    }
    data = xcalloc(1, sizeof(icpUdpData));
    data->address = *to;
    data->msg = buf;
    data->len = buf_len;
    data->start = start_time;
    data->logcode = LOG_UDP_HIT_OBJ;
    data->proto = urlParseProtocol(entry->url);
    debug(12, 4, "icpUdpSendEntry: Queueing for %s: \"%s %s\"\n",
	inet_ntoa(to->sin_addr),
	IcpOpcodeStr[opcode],
	url);
    AppendUdp(data);
    comm_set_select_handler(fd,
	COMM_SELECT_WRITE,
	(PF) icpUdpReply,
	(void *) UdpQueueHead);
}

static void
icpHitObjHandler(int errflag, void *data)
{
    icpHitObjStateData *icpHitObjState = data;
    StoreEntry *entry = NULL;
    if (data == NULL)
	return;
    entry = icpHitObjState->entry;
    debug(12, 3, "icpHitObjHandler: '%s'\n", entry->url);
    if (!errflag) {
	icpUdpSendEntry(icpHitObjState->fd,
	    entry->url,
	    &icpHitObjState->header,
	    &icpHitObjState->to,
	    ICP_OP_HIT_OBJ,
	    entry,
	    icpHitObjState->started);
    } else {
	debug(12, 3, "icpHitObjHandler: errflag=%d, aborted!\n", errflag);
    }
    storeUnlockObject(entry);
    safe_free(icpHitObjState);
}

static void
icpHandleIcpV2(int fd, struct sockaddr_in from, char *buf, int len)
{
    icp_common_t header;
    icp_common_t *headerp = (icp_common_t *) (void *) buf;
    StoreEntry *entry = NULL;
    char *url = NULL;
    char *key = NULL;
    request_t *icp_request = NULL;
    int allow = 0;
    char *data = NULL;
    u_short data_sz = 0;
    u_short u;
    icpHitObjStateData *icpHitObjState = NULL;
    int pkt_len;
    protocol_t p;
    aclCheck_t checklist;

    header.opcode = headerp->opcode;
    header.version = headerp->version;
    header.length = ntohs(headerp->length);
    header.reqnum = ntohl(headerp->reqnum);
    header.flags = ntohl(headerp->flags);
    /* xmemcpy(headerp->auth, , ICP_AUTH_SIZE); */
    header.shostid = ntohl(headerp->shostid);

    switch (header.opcode) {
    case ICP_OP_QUERY:
	nudpconn++;
	/* We have a valid packet */
	url = buf + sizeof(header) + sizeof(u_num32);
	if ((icp_request = urlParse(METHOD_GET, url)) == NULL) {
	    icpUdpSend(fd,
		url,
		&header,
		&from,
		0,
		ICP_OP_ERR,
		LOG_UDP_INVALID,
		PROTO_NONE);
	    break;
	}
	p = icp_request->protocol;
	checklist.src_addr = from.sin_addr;
	checklist.request = icp_request;
	allow = aclCheck(ICPAccessList, &checklist);
	put_free_request_t(icp_request);
	if (!allow) {
	    debug(12, 2, "icpHandleIcpV2: Access Denied for %s by %s.\n",
		inet_ntoa(from.sin_addr), AclMatchedName);
	    icpUdpSend(fd,
		url,
		&header,
		&from,
		0,
		ICP_OP_DENIED,
		LOG_UDP_DENIED,
		p);
	    break;
	}
	/* The peer is allowed to use this cache */
	entry = storeGet(storeGeneratePublicKey(url, METHOD_GET));
	debug(12, 5, "icpHandleIcpV2: OPCODE %s\n", IcpOpcodeStr[header.opcode]);
	if (entry &&
	    (entry->store_status == STORE_OK) &&
	    (entry->expires > (squid_curtime + Config.negativeTtl))) {
	    pkt_len = sizeof(icp_common_t) + strlen(url) + 1 + 2 + entry->object_len;
	    if (header.flags & ICP_FLAG_HIT_OBJ && pkt_len < SQUID_UDP_SO_SNDBUF) {
		icpHitObjState = xcalloc(1, sizeof(icpHitObjStateData));
		icpHitObjState->entry = entry;
		icpHitObjState->fd = fd;
		icpHitObjState->to = from;
		icpHitObjState->header = header;
		icpHitObjState->started = current_time;
		if (storeLockObject(entry, icpHitObjHandler, icpHitObjState) == 0)
		    break;
		/* else, problems */
		safe_free(icpHitObjState);
	    }
	    icpUdpSend(fd,
		url,
		&header,
		&from,
		0,
		ICP_OP_HIT,
		LOG_UDP_HIT,
		p);
	    break;
	}
	/* if store is rebuilding, return a UDP_HIT, but not a MISS */
	if (opt_reload_hit_only && store_rebuilding == STORE_REBUILDING_FAST) {
	    icpUdpSend(fd,
		url,
		&header,
		&from,
		0,
		ICP_OP_RELOADING,
		LOG_UDP_RELOADING,
		p);
	    break;
	}
	icpUdpSend(fd,
	    url,
	    &header,
	    &from,
	    0,
	    ICP_OP_MISS,
	    LOG_UDP_MISS,
	    p);
	break;

    case ICP_OP_HIT_OBJ:
    case ICP_OP_HIT:
    case ICP_OP_SECHO:
    case ICP_OP_DECHO:
    case ICP_OP_MISS:
    case ICP_OP_DENIED:
    case ICP_OP_RELOADING:
	if (neighbors_do_private_keys && header.reqnum == 0) {
	    debug(12, 0, "icpHandleIcpV2: Neighbor %s returned reqnum = 0\n",
		inet_ntoa(from.sin_addr));
	    debug(12, 0, "icpHandleIcpV2: Disabling use of private keys\n");
	    neighbors_do_private_keys = 0;
	}
	url = buf + sizeof(header);
	if (header.opcode == ICP_OP_HIT_OBJ) {
	    data = url + strlen(url) + 1;
	    xmemcpy((char *) &u, data, sizeof(u_short));
	    data += sizeof(u_short);
	    data_sz = ntohs(u);
	    if ((int) data_sz > (len - (data - buf))) {
		debug(12, 0, "icpHandleIcpV2: ICP_OP_HIT_OBJ object too small\n");
		break;
	    }
	}
	debug(12, 3, "icpHandleIcpV2: %s from %s for '%s'\n",
	    IcpOpcodeStr[header.opcode],
	    inet_ntoa(from.sin_addr),
	    url);
	if (neighbors_do_private_keys && header.reqnum) {
	    key = storeGeneratePrivateKey(url, METHOD_GET, header.reqnum);
	} else {
	    key = storeGeneratePublicKey(url, METHOD_GET);
	}
	debug(12, 3, "icpHandleIcpV2: Looking for key '%s'\n", key);
	if ((entry = storeGet(key)) == NULL) {
	    debug(12, 3, "icpHandleIcpV2: Ignoring %s for NULL Entry.\n",
		IcpOpcodeStr[header.opcode]);
	} else if (entry->lock_count == 0) {
	    debug(12, 3, "icpHandleIcpV2: Ignoring %s for Entry without locks.\n",
		IcpOpcodeStr[header.opcode]);
	} else {
	    /* call neighborsUdpAck even if ping_status != PING_WAITING */
	    neighborsUdpAck(fd,
		url,
		&header,
		&from,
		entry,
		data,
		(int) data_sz);
	}
	break;

    case ICP_OP_INVALID:
	break;

    default:
	debug(12, 0, "icpHandleIcpV2: UNKNOWN OPCODE: %d from %s\n",
	    header.opcode, inet_ntoa(from.sin_addr));
	break;
    }
}

/* Currently Harvest cached-2.x uses ICP_VERSION_3 */
static void
icpHandleIcpV3(int fd, struct sockaddr_in from, char *buf, int len)
{
    icp_common_t header;
    icp_common_t *headerp = (icp_common_t *) (void *) buf;
    StoreEntry *entry = NULL;
    char *url = NULL;
    char *key = NULL;
    request_t *icp_request = NULL;
    int allow = 0;
    char *data = NULL;
    u_short data_sz = 0;
    u_short u;
    protocol_t p;
    aclCheck_t checklist;

    header.opcode = headerp->opcode;
    header.version = headerp->version;
    header.length = ntohs(headerp->length);
    header.reqnum = ntohl(headerp->reqnum);
    header.flags = ntohl(headerp->flags);
    /* xmemcpy(headerp->auth, , ICP_AUTH_SIZE); */
    header.shostid = ntohl(headerp->shostid);

    switch (header.opcode) {
    case ICP_OP_QUERY:
	nudpconn++;
	/* We have a valid packet */
	url = buf + sizeof(header) + sizeof(u_num32);
	if ((icp_request = urlParse(METHOD_GET, url)) == NULL) {
	    icpUdpSend(fd,
		url,
		&header,
		&from,
		0,
		ICP_OP_INVALID,
		LOG_UDP_INVALID,
		PROTO_NONE);
	    break;
	}
	p = icp_request->protocol;
	checklist.src_addr = from.sin_addr;
	checklist.request = icp_request;
	allow = aclCheck(ICPAccessList, &checklist);
	put_free_request_t(icp_request);
	if (!allow) {
	    debug(12, 2, "icpHandleIcpV3: Access Denied for %s by %s.\n",
		inet_ntoa(from.sin_addr), AclMatchedName);
	    icpUdpSend(fd,
		url,
		&header,
		&from,
		0,
		ICP_OP_DENIED,
		LOG_UDP_DENIED,
		p);
	    break;
	}
	/* The peer is allowed to use this cache */
	entry = storeGet(storeGeneratePublicKey(url, METHOD_GET));
	debug(12, 5, "icpHandleIcpV3: OPCODE %s\n",
	    IcpOpcodeStr[header.opcode]);
	if (entry &&
	    (entry->store_status == STORE_OK) &&
	    (entry->expires > (squid_curtime + Config.negativeTtl))) {
	    icpUdpSend(fd,
		url,
		&header,
		&from,
		0,
		ICP_OP_HIT,
		LOG_UDP_HIT,
		p);
	    break;
	}
	/* if store is rebuilding, return a UDP_HIT, but not a MISS */
	if (opt_reload_hit_only && store_rebuilding == STORE_REBUILDING_FAST) {
	    icpUdpSend(fd,
		url,
		&header,
		&from,
		0,
		ICP_OP_DENIED,
		LOG_UDP_DENIED,
		p);
	    break;
	}
	icpUdpSend(fd,
	    url,
	    &header,
	    &from,
	    0,
	    ICP_OP_MISS,
	    LOG_UDP_MISS,
	    p);
	break;

    case ICP_OP_HIT_OBJ:
    case ICP_OP_HIT:
    case ICP_OP_SECHO:
    case ICP_OP_DECHO:
    case ICP_OP_MISS:
    case ICP_OP_DENIED:
	if (neighbors_do_private_keys && header.reqnum == 0) {
	    debug(12, 0, "icpHandleIcpV3: Neighbor %s returned reqnum = 0\n",
		inet_ntoa(from.sin_addr));
	    debug(12, 0, "icpHandleIcpV3: Disabling use of private keys\n");
	    neighbors_do_private_keys = 0;
	}
	url = buf + sizeof(header);
	if (header.opcode == ICP_OP_HIT_OBJ) {
	    data = url + strlen(url) + 1;
	    xmemcpy((char *) &u, data, sizeof(u_short));
	    data += sizeof(u_short);
	    data_sz = ntohs(u);
	    if ((int) data_sz > (len - (data - buf))) {
		debug(12, 0, "icpHandleIcpV3: ICP_OP_HIT_OBJ object too small\n");
		break;
	    }
	}
	debug(12, 3, "icpHandleIcpV3: %s from %s for '%s'\n",
	    IcpOpcodeStr[header.opcode],
	    inet_ntoa(from.sin_addr),
	    url);
	if (neighbors_do_private_keys && header.reqnum) {
	    key = storeGeneratePrivateKey(url, METHOD_GET, header.reqnum);
	} else {
	    key = storeGeneratePublicKey(url, METHOD_GET);
	}
	debug(12, 3, "icpHandleIcpV3: Looking for key '%s'\n", key);
	if ((entry = storeGet(key)) == NULL) {
	    debug(12, 3, "icpHandleIcpV3: Ignoring %s for NULL Entry.\n",
		IcpOpcodeStr[header.opcode]);
	} else if (entry->lock_count == 0) {
	    debug(12, 3, "icpHandleIcpV3: Ignoring %s for Entry without locks.\n",
		IcpOpcodeStr[header.opcode]);
	} else {
	    /* call neighborsUdpAck even if ping_status != PING_WAITING */
	    neighborsUdpAck(fd,
		url,
		&header,
		&from,
		entry,
		data,
		(int) data_sz);
	}
	break;

    case ICP_OP_INVALID:
    case ICP_OP_ERR:
	break;

    default:
	debug(12, 0, "icpHandleIcpV3: UNKNOWN OPCODE: %d from %s\n",
	    header.opcode, inet_ntoa(from.sin_addr));
	break;
    }
}

#ifdef ICP_PKT_DUMP
static void
icpPktDump(icp_common_t * pkt)
{
    struct in_addr a;
    debug(12, 9, "opcode:     %3d %s\n",
	(int) pkt->opcode,
	IcpOpcodeStr[pkt->opcode]);
    debug(12, 9, "version: %-8d\n", (int) pkt->version);
    debug(12, 9, "length:  %-8d\n", (int) ntohs(pkt->length));
    debug(12, 9, "reqnum:  %-8d\n", ntohl(pkt->reqnum));
    debug(12, 9, "flags:   %-8x\n", ntohl(pkt->flags));
    a.s_addr = ntohl(pkt->shostid);
    debug(12, 9, "shostid: %s\n", inet_ntoa(a));
    debug(12, 9, "payload: %s\n", (char *) pkt + sizeof(icp_common_t));
}
#endif

int
icpHandleUdp(int sock, void *not_used)
{
    int result = 0;
    struct sockaddr_in from;
    int from_len;
    LOCAL_ARRAY(char, buf, SQUID_UDP_SO_RCVBUF);
    int len;
    icp_common_t *headerp = NULL;
    int icp_version;

    from_len = sizeof(from);
    memset(&from, 0, from_len);
    len = comm_udp_recv(sock, buf, SQUID_UDP_SO_RCVBUF - 1, &from, &from_len);
    if (len < 0) {
	debug(12, 1, "icpHandleUdp: FD %d: error receiving.\n", sock);
	comm_set_select_handler(sock, COMM_SELECT_READ, icpHandleUdp, 0);
	return result;
    }
    buf[len] = '\0';
    debug(12, 4, "icpHandleUdp: FD %d: received %d bytes from %s.\n",
	sock,
	len,
	inet_ntoa(from.sin_addr));
#ifdef ICP_PACKET_DUMP
    icpPktDump(buf);
#endif
    if (len < sizeof(icp_common_t)) {
	debug(12, 4, "icpHandleUdp: Bad sized UDP packet ignored. %d < %d\n",
	    len, sizeof(icp_common_t));
	comm_set_select_handler(sock, COMM_SELECT_READ, icpHandleUdp, 0);
	return result;
    }
    headerp = (icp_common_t *) (void *) buf;
    if ((icp_version = (int) headerp->version) == ICP_VERSION_2)
	icpHandleIcpV2(sock, from, buf, len);
    else if (icp_version == ICP_VERSION_3)
	icpHandleIcpV3(sock, from, buf, len);
    else
	debug(12, 0, "Unused ICP version %d received from %s:%d\n",
	    icp_version,
	    inet_ntoa(from.sin_addr),
	    ntohs(from.sin_port));

    comm_set_select_handler(sock,
	COMM_SELECT_READ,
	icpHandleUdp,
	0);
    return result;
}

static char *
do_append_domain(char *url, char *ad)
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
 *  parseHttpRequest()
 * 
 *  Called by
 *    asciiProcessInput() after the request has been read
 *  Calls
 *    mime_process()
 *    do_append_domain()
 *  Returns
 *   -1 on error
 *    0 on incomplete request
 *    1 on success
 */
static int
parseHttpRequest(icpStateData * icpState)
{
    char *inbuf = NULL;
    char *method = NULL;
    char *request = NULL;
    char *req_hdr = NULL;
    LOCAL_ARRAY(char, http_ver, 32);
    char *token = NULL;
    char *t = NULL;
    char *ad = NULL;
    char *post_data = NULL;
    int free_request = 0;
    int content_length;
    int req_hdr_sz;
    int post_sz;
    int len;

    /* Make sure a complete line has been received */
    if (strchr(icpState->inbuf, '\n') == NULL) {
	debug(12, 5, "Incomplete request line, waiting for more data\n");
	return 0;
    }
    /* Use xmalloc/xmemcpy instead of xstrdup because inbuf might
     * contain NULL bytes; especially for POST data  */
    inbuf = xmalloc(icpState->offset + 1);
    xmemcpy(inbuf, icpState->inbuf, icpState->offset);
    *(inbuf + icpState->offset) = '\0';

    /* Look for request method */
    if ((method = strtok(inbuf, "\t ")) == NULL) {
	debug(12, 1, "parseHttpRequest: Can't get request method\n");
	xfree(inbuf);
	return -1;
    }
    icpState->method = urlParseMethod(method);
    if (icpState->method == METHOD_NONE) {
	debug(12, 1, "parseHttpRequest: Unsupported method '%s'\n", method);
	xfree(inbuf);
	return -1;
    }
    debug(12, 5, "parseHttpRequest: Method is '%s'\n", method);
    BIT_SET(icpState->flags, REQ_HTML);

    /* look for URL */
    if ((request = strtok(NULL, "\r\n\t ")) == NULL) {
	debug(12, 1, "parseHttpRequest: Missing URL\n");
	xfree(inbuf);
	return -1;
    }
    debug(12, 5, "parseHttpRequest: Request is '%s'\n", request);

    token = strtok(NULL, "");
    for (t = token; t && *t && *t != '\n' && *t != '\r'; t++);
    if (t == NULL || *t == '\0' || t == token) {
	debug(12, 3, "parseHttpRequest: Missing HTTP identifier\n");
	xfree(inbuf);
	return -1;
    }
    len = (int) (t - token);
    memset(http_ver, '\0', 32);
    strncpy(http_ver, token, len < 31 ? len : 31);
    sscanf(http_ver, "%f", &icpState->http_ver);
    debug(12, 5, "parseHttpRequest: HTTP version is '%s'\n", http_ver);

    req_hdr = t;
    req_hdr_sz = icpState->offset - (req_hdr - inbuf);

    /* Check if headers are received */
    if (!mime_headers_end(req_hdr)) {
	xfree(inbuf);
	return 0;		/* not a complete request */
    }
    /* Ok, all headers are received */
    icpState->request_hdr = xmalloc(req_hdr_sz + 1);
    xmemcpy(icpState->request_hdr, req_hdr, req_hdr_sz);
    *(icpState->request_hdr + req_hdr_sz) = '\0';

    debug(12, 5, "parseHttpRequest: Request Header is\n---\n%s\n---\n",
	icpState->request_hdr);

    if (icpState->method == METHOD_POST || icpState->method == METHOD_PUT) {
	/* Expect Content-Length: and POST data after the headers */
	if ((t = mime_get_header(req_hdr, "Content-Length")) == NULL) {
	    debug(12, 2, "POST without Content-Length\n");
	    xfree(inbuf);
	    return -1;
	}
	content_length = atoi(t);
	debug(12, 3, "parseHttpRequest: Expecting POST/PUT Content-Length of %d\n",
	    content_length);
	if (!(post_data = mime_headers_end(req_hdr))) {
	    debug(12, 1, "parseHttpRequest: Can't find end of headers in POST/PUT request?\n");
	    xfree(inbuf);
	    return 0;		/* not a complete request */
	}
	post_sz = icpState->offset - (post_data - inbuf);
	debug(12, 3, "parseHttpRequest: Found POST/PUT Content-Length of %d\n",
	    post_sz);
	if (post_sz < content_length) {
	    xfree(inbuf);
	    return 0;
	}
    }
    /* Assign icpState->url */
    if ((t = strchr(request, '\n')))	/* remove NL */
	*t = '\0';
    if ((t = strchr(request, '\r')))	/* remove CR */
	*t = '\0';
    if ((t = strchr(request, '#')))	/* remove HTML anchors */
	*t = '\0';

    if ((ad = Config.appendDomain)) {
	if ((t = do_append_domain(request, ad))) {
	    if (free_request)
		safe_free(request);
	    request = t;
	    free_request = 1;
	    /* NOTE: We don't have to free the old request pointer
	     * because it points to inside inbuf. But
	     * do_append_domain() allocates new memory so set a flag
	     * if the request should be freed later. */
	}
    }
    /* see if we running in httpd_accel_mode, if so got to convert it to URL */
    if (httpd_accel_mode && *request == '/') {
	if (!vhost_mode) {
	    /* prepend the accel prefix */
	    icpState->url = xcalloc(strlen(Config.Accel.prefix) +
		strlen(request) +
		1, 1);
	    sprintf(icpState->url, "%s%s", Config.Accel.prefix, request);
	} else {
	    /* Put the local socket IP address as the hostname */
	    icpState->url = xcalloc(strlen(request) + 24, 1);
	    sprintf(icpState->url, "http://%s:%d%s",
		inet_ntoa(icpState->me.sin_addr),
		Config.Accel.port,
		request);
	    debug(12, 5, "VHOST REWRITE: '%s'\n", icpState->url);
	}
	BIT_SET(icpState->flags, REQ_ACCEL);
    } else {
	/* URL may be rewritten later, so make extra room */
	icpState->url = xcalloc(strlen(request) + 5, 1);
	strcpy(icpState->url, request);
	BIT_RESET(icpState->flags, REQ_ACCEL);
    }

    debug(12, 5, "parseHttpRequest: Complete request received\n");
    if (free_request)
	safe_free(request);
    xfree(inbuf);
    return 1;
}

#define ASCII_INBUF_BLOCKSIZE 4096
/*
 * asciiProcessInput()
 * 
 * Handler set by
 *   asciiHandleConn()
 * Called by
 *   comm_select() when data has been read
 * Calls
 *   parseAsciiUrl()
 *   icpProcessRequest()
 *   icpSendERROR()
 */
static void
asciiProcessInput(int fd, char *buf, int size, int flag, void *data)
{
    icpStateData *icpState = (icpStateData *) data;
    int parser_return_code = 0;
    int k;
    request_t *request = NULL;
    char *wbuf = NULL;

    debug(12, 4, "asciiProcessInput: FD %d: reading request...\n", fd);
    debug(12, 4, "asciiProcessInput: size = %d\n", size);

    if (flag != COMM_OK) {
	/* connection closed by foreign host */
	comm_close(fd);
	return;
    }
    icpState->offset += size;
    icpState->inbuf[icpState->offset] = '\0';	/* Terminate the string */

    parser_return_code = parseHttpRequest(icpState);
    if (parser_return_code == 1) {
	if ((request = urlParse(icpState->method, icpState->url)) == NULL) {
	    if (strstr(icpState->url, "/echo")) {
		debug(12, 0, "ECHO request from %s\n",
		    inet_ntoa(icpState->peer.sin_addr));
		icpSendERROR(fd, LOG_TCP_MISS, icpState->request_hdr, icpState, 200);
	    } else {
		debug(12, 5, "Invalid URL: %s\n", icpState->url);
		wbuf = squid_error_url(icpState->url,
		    icpState->method,
		    ERR_INVALID_URL,
		    fd_table[fd].ipaddr,
		    icpState->http_code,
		    NULL);
		icpSendERROR(fd, ERR_INVALID_URL, wbuf, icpState, 400);
	    }
	    return;
	}
	if (!urlCheckRequest(request)) {
	    icpState->log_type = ERR_UNSUP_REQ;
	    icpState->http_code = 501;
	    icpState->buf = xstrdup(squid_error_url(icpState->url,
		    icpState->method,
		    ERR_UNSUP_REQ,
		    fd_table[fd].ipaddr,
		    icpState->http_code,
		    NULL));
	    comm_write(fd,
		icpState->buf,
		strlen(icpState->buf),
		30,
		icpSendERRORComplete,
		(void *) icpState,
		xfree);
	    return;
	}
	icpState->request = requestLink(request);
	clientAccessCheck(icpState, clientAccessCheckDone);
    } else if (parser_return_code == 0) {
	/*
	 *    Partial request received; reschedule until parseAsciiUrl()
	 *    is happy with the input
	 */
	k = icpState->inbufsize - 1 - icpState->offset;
	if (k == 0) {
	    if (icpState->offset >= Config.maxRequestSize) {
		/* The request is too large to handle */
		debug(12, 0, "asciiProcessInput: Request won't fit in buffer.\n");
		debug(12, 0, "-->     max size = %d\n", Config.maxRequestSize);
		debug(12, 0, "--> icpState->offset = %d\n", icpState->offset);
		icpSendERROR(fd,
		    ERR_INVALID_REQ,
		    "error reading request",
		    icpState,
		    400);
		return;
	    }
	    /* Grow the request memory area to accomodate for a large request */
	    icpState->inbufsize += ASCII_INBUF_BLOCKSIZE;
	    icpState->inbuf = xrealloc(icpState->inbuf, icpState->inbufsize);
	    meta_data.misc += ASCII_INBUF_BLOCKSIZE;
	    debug(12, 2, "Handling a large request, offset=%d inbufsize=%d\n",
		icpState->offset, icpState->inbufsize);
	    k = icpState->inbufsize - 1 - icpState->offset;
	}
	comm_read(fd,
	    icpState->inbuf + icpState->offset,
	    k,			/* size */
	    30,			/* timeout */
	    TRUE,		/* handle immed */
	    asciiProcessInput,
	    (void *) icpState);
    } else {
	/* parser returned -1 */
	debug(12, 1, "asciiProcessInput: FD %d Invalid Request\n", fd);
	wbuf = squid_error_request(icpState->inbuf,
	    ERR_INVALID_REQ,
	    fd_table[fd].ipaddr,
	    icpState->http_code);
	icpSendERROR(fd, ERR_INVALID_REQ, wbuf, icpState, 400);
    }
}


/* general lifetime handler for ascii connection */
static void
asciiConnLifetimeHandle(int fd, icpStateData * icpState)
{
    debug(12, 2, "asciiConnLifetimeHandle: FD %d: lifetime is expired.\n", fd);
    CheckQuickAbort(icpState);
    /* Unregister us from the dnsserver pending list and cause a DNS
     * related storeAbort() for other attached clients.  If this
     * doesn't succeed, then the fetch has already started for this
     * URL. */
    protoUnregister(fd,
	icpState->entry,
	icpState->request,
	icpState->peer.sin_addr);
    comm_close(fd);
}

/* Handle a new connection on ascii input socket. */
int
asciiHandleConn(int sock, void *notused)
{
    int fd = -1;
    int lft = -1;
    icpStateData *icpState = NULL;
    struct sockaddr_in peer;
    struct sockaddr_in me;

    if ((fd = comm_accept(sock, &peer, &me)) < 0) {
	debug(12, 1, "asciiHandleConn: FD %d: accept failure: %s\n",
	    sock, xstrerror());
	comm_set_select_handler(sock, COMM_SELECT_READ, asciiHandleConn, 0);
	return -1;
    }
    /* set the hardwired lifetime */
    lft = comm_set_fd_lifetime(fd, Config.lifetimeDefault);
    ntcpconn++;

    debug(12, 4, "asciiHandleConn: FD %d: accepted, lifetime %d\n", fd, lft);

    icpState = xcalloc(1, sizeof(icpStateData));
    icpState->start = current_time;
    icpState->inbufsize = ASCII_INBUF_BLOCKSIZE;
    icpState->inbuf = xcalloc(icpState->inbufsize, 1);
    icpState->header.shostid = htonl(peer.sin_addr.s_addr);
    icpState->peer = peer;
    icpState->log_addr = peer.sin_addr;
    icpState->log_addr.s_addr &= Config.Addrs.client_netmask.s_addr;
    icpState->me = me;
    icpState->entry = NULL;
    icpState->fd = fd;
    fd_note(fd, inet_ntoa(icpState->log_addr));
    meta_data.misc += ASCII_INBUF_BLOCKSIZE;
    comm_set_select_handler(fd,
	COMM_SELECT_LIFETIME,
	(PF) asciiConnLifetimeHandle,
	(void *) icpState);
    comm_add_close_handler(fd,
	(PF) icpStateFree,
	(void *) icpState);
    comm_read(fd,
	icpState->inbuf,
	icpState->inbufsize - 1,	/* size */
	30,			/* timeout */
	1,			/* handle immed */
	asciiProcessInput,
	(void *) icpState);
    comm_set_select_handler(sock,
	COMM_SELECT_READ,
	asciiHandleConn,
	0);
    if (Config.identLookup)
	identStart(-1, icpState);
    /* start reverse lookup */
    if (Config.Log.log_fqdn)
	fqdncache_gethostbyaddr(peer.sin_addr, FQDN_LOOKUP_IF_MISS);
    return 0;
}

void
AppendUdp(icpUdpData * item)
{
    item->next = NULL;
    if (UdpQueueHead == NULL) {
	UdpQueueHead = item;
	UdpQueueTail = item;
    } else if (UdpQueueTail == UdpQueueHead) {
	UdpQueueTail = item;
	UdpQueueHead->next = item;
    } else {
	UdpQueueTail->next = item;
	UdpQueueTail = item;
    }
}

/* return 1 if the request should be aborted */
static int
CheckQuickAbort2(icpStateData * icpState)
{
    long curlen;
    long minlen;
    long expectlen;
    if (!BIT_TEST(icpState->flags, REQ_CACHABLE))
	return 1;
    if (BIT_TEST(icpState->entry->flag, KEY_PRIVATE))
	return 1;
    if (icpState->entry->mem_obj == NULL)
	return 1;
    expectlen = icpState->entry->mem_obj->reply->content_length;
    curlen = icpState->entry->mem_obj->e_current_len;
    minlen = Config.quickAbort.min;
    if (minlen < 0)
	/* disabled */
	return 0;
    if (curlen > expectlen)
	/* bad content length */
	return 1;
    if ((expectlen - curlen) < minlen)
	/* only little more left */
	return 0;
    if ((expectlen - curlen) > Config.quickAbort.max)
	/* too much left to go */
	return 1;
    if ((curlen / (expectlen / 128U)) > Config.quickAbort.pct)
	/* past point of no return */
	return 0;
    return 1;
}


static void
CheckQuickAbort(icpStateData * icpState)
{
    if (icpState->entry == NULL)
	return;
    if (storePendingNClients(icpState->entry) > 1)
	return;
    if (icpState->entry->store_status == STORE_OK)
	return;
    if (CheckQuickAbort2(icpState) == 0)
	return;
    BIT_SET(icpState->entry->flag, CLIENT_ABORT_REQUEST);
    storeReleaseRequest(icpState->entry);
    icpState->log_type = ERR_CLIENT_ABORT;
}

void
icpDetectClientClose(int fd, icpStateData * icpState)
{
    LOCAL_ARRAY(char, buf, 256);
    int n;
    StoreEntry *entry = icpState->entry;
    errno = 0;
    n = read(fd, buf, 256);
    if (n > 0) {
	debug(12, 0, "icpDetectClientClose: FD %d, %d unexpected bytes\n",
	    fd, n);
	comm_set_select_handler(fd,
	    COMM_SELECT_READ,
	    (PF) icpDetectClientClose,
	    (void *) icpState);
    } else if (n < 0) {
	debug(12, 5, "icpDetectClientClose: FD %d\n", fd);
	debug(12, 5, "--> URL '%s'\n", icpState->url);
	if (errno == ECONNRESET)
	    debug(12, 2, "icpDetectClientClose: ERROR %s\n", xstrerror());
	else
	    debug(12, 1, "icpDetectClientClose: ERROR %s\n", xstrerror());
	CheckQuickAbort(icpState);
	protoUnregister(fd, entry, icpState->request, icpState->peer.sin_addr);
	if (entry && entry->ping_status == PING_WAITING)
	    storeReleaseRequest(entry);
	comm_close(fd);
    } else if (entry != NULL && icpState->offset == entry->object_len &&
	entry->store_status != STORE_PENDING) {
	/* All data has been delivered */
	debug(12, 5, "icpDetectClientClose: FD %d end of transmission\n", fd);
	HTTPCacheInfo->proto_touchobject(HTTPCacheInfo,
	    HTTPCacheInfo->proto_id(entry->url),
	    icpState->offset);
	comm_close(fd);
    } else {
	debug(12, 5, "icpDetectClientClose: FD %d closed?\n", fd);
	comm_set_stall(fd, 60);	/* check again in a minute */
    }
}

static char *
icpConstruct304reply(struct _http_reply *source)
{
    LOCAL_ARRAY(char, line, 256);
    LOCAL_ARRAY(char, reply, 8192);
    memset(reply, '\0', 8192);
    strcpy(reply, "HTTP/1.0 304 Not Modified\r\n");
    if (strlen(source->date) > 0) {
	sprintf(line, "Date: %s\n", source->date);
	strcat(reply, line);
    }
    if (strlen(source->content_type) > 0) {
	sprintf(line, "Content-type: %s\n", source->content_type);
	strcat(reply, line);
    }
    if (source->content_length) {
	sprintf(line, "Content-length: %d\n", source->content_length);
	strcat(reply, line);
    }
    if (strlen(source->expires) > 0) {
	sprintf(line, "Expires: %s\n", source->expires);
	strcat(reply, line);
    }
    if (strlen(source->last_modified) > 0) {
	sprintf(line, "Last-modified: %s\n", source->last_modified);
	strcat(reply, line);
    }
    sprintf(line, "\r\n");
    strcat(reply, line);
    return reply;
}


/*
 * $Id$
 *
 * DEBUG: section 24    WAIS Relay
 * AUTHOR: Harvest Derived
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

typedef struct {
    int fd;
    StoreEntry *entry;
    method_t method;
    char *relayhost;
    int relayport;
    char *request_hdr;
    char request[MAX_URL];
} WaisStateData;

static PF waisStateFree;
static PF waisTimeout;
static PF waisReadReply;
static CWCB waisSendComplete;
static PF waisSendRequest;
static CNCB waisConnectDone;
static STABH waisAbort;

static void
waisStateFree(int fd, void *data)
{
    WaisStateData *waisState = data;
    if (waisState == NULL)
	return;
    storeUnregisterAbort(waisState->entry);
    storeUnlockObject(waisState->entry);
    cbdataFree(waisState);
}

/* This will be called when socket lifetime is expired. */
static void
waisTimeout(int fd, void *data)
{
    WaisStateData *waisState = data;
    ErrorState *err;
    StoreEntry *entry = waisState->entry;
    debug(24, 4) ("waisTimeout: FD %d: '%s'\n", fd, entry->url);
    /* was assert */
    err = xcalloc(1, sizeof(ErrorState));
    err->type = ERR_READ_TIMEOUT;
    err->http_status = HTTP_GATEWAY_TIMEOUT;
    err->request = urlParse(METHOD_CONNECT, waisState->request);
    errorAppendEntry(entry, err);

    storeAbort(entry, 0);
    comm_close(fd);
}



/* This will be called when data is ready to be read from fd.  Read until
 * error or connection closed. */
static void
waisReadReply(int fd, void *data)
{
    WaisStateData *waisState = data;
    LOCAL_ARRAY(char, buf, 4096);
    StoreEntry *entry = waisState->entry;
    int len;
    int clen;
    int off;
    int bin;
    if (protoAbortFetch(entry)) {
	ErrorState *err;
	err = xcalloc(1, sizeof(ErrorState));
	err->type = ERR_CLIENT_ABORT;
	err->http_status = HTTP_INTERNAL_SERVER_ERROR;
	err->request = urlParse(METHOD_CONNECT, waisState->request);
	errorAppendEntry(entry, err);
	storeAbort(entry, 0);
	comm_close(fd);
	return;
    }
    /* check if we want to defer reading */
    clen = entry->mem_obj->inmem_hi;
    off = storeLowestMemReaderOffset(entry);
    len = read(fd, buf, 4096);
    fd_bytes(fd, len, FD_READ);
    debug(24, 5) ("waisReadReply: FD %d read len:%d\n", fd, len);
    if (len > 0) {
	commSetTimeout(fd, Config.Timeout.read, NULL, NULL);
	IOStats.Wais.reads++;
	for (clen = len - 1, bin = 0; clen; bin++)
	    clen >>= 1;
	IOStats.Wais.read_hist[bin]++;
    }
    if (len < 0) {
	debug(50, 1) ("waisReadReply: FD %d: read failure: %s.\n", xstrerror());
	if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
	    /* reinstall handlers */
	    /* XXX This may loop forever */
	    commSetSelect(fd, COMM_SELECT_READ,
		waisReadReply, waisState, 0);
	} else {
	    ErrorState *err;
	    BIT_RESET(entry->flag, ENTRY_CACHABLE);
	    storeReleaseRequest(entry);
	    /* was assert */

	    err = xcalloc(1, sizeof(ErrorState));
	    err->type = ERR_READ_ERROR;
	    err->http_status = HTTP_INTERNAL_SERVER_ERROR;
	    err->request = urlParse(METHOD_CONNECT, waisState->request);
	    errorAppendEntry(entry, err);

	    storeAbort(entry, 0);
	    comm_close(fd);
	}
    } else if (len == 0 && entry->mem_obj->inmem_hi == 0) {
	/* was assert */
	ErrorState *err;
	err = xcalloc(1, sizeof(ErrorState));
	err->type = ERR_ZERO_SIZE_OBJECT;
	err->errno = errno;
	err->http_status = HTTP_SERVICE_UNAVAILABLE;
	err->request = urlParse(METHOD_CONNECT, waisState->request);
	errorAppendEntry(entry, err);
	storeAbort(entry, 0);
	comm_close(fd);
    } else if (len == 0) {
	/* Connection closed; retrieval done. */
	entry->expires = squid_curtime;
	storeComplete(entry);
	comm_close(fd);
    } else {
	storeAppend(entry, buf, len);
	commSetSelect(fd,
	    COMM_SELECT_READ,
	    waisReadReply,
	    waisState, 0);
    }
}

/* This will be called when request write is complete. Schedule read of
 * reply. */
static void
waisSendComplete(int fd, char *buf, int size, int errflag, void *data)
{
    WaisStateData *waisState = data;
    StoreEntry *entry = waisState->entry;
    debug(24, 5) ("waisSendComplete: FD %d size: %d errflag: %d\n",
	fd, size, errflag);
    if (errflag) {
	/* was assert */
	ErrorState *err;
	err = xcalloc(1, sizeof(ErrorState));
	err->type = ERR_CONNECT_FAIL;
	err->errno = errno;
	err->host = xstrdup(waisState->relayhost);
	err->port = waisState->relayport;
	err->http_status = HTTP_SERVICE_UNAVAILABLE;
	err->request = urlParse(METHOD_CONNECT, waisState->request);
	errorAppendEntry(entry, err);

	storeAbort(entry, 0);
	comm_close(fd);
    } else {
	/* Schedule read reply. */
	commSetSelect(fd,
	    COMM_SELECT_READ,
	    waisReadReply,
	    waisState, 0);
	commSetDefer(fd, protoCheckDeferRead, entry);
    }
}

/* This will be called when connect completes. Write request. */
static void
waisSendRequest(int fd, void *data)
{
    WaisStateData *waisState = data;
    int len = strlen(waisState->request) + 4;
    char *buf = NULL;
    const char *Method = RequestMethodStr[waisState->method];

    debug(24, 5) ("waisSendRequest: FD %d\n", fd);

    if (Method)
	len += strlen(Method);
    if (waisState->request_hdr)
	len += strlen(waisState->request_hdr);

    buf = xcalloc(1, len + 1);

    if (waisState->request_hdr)
	snprintf(buf, len + 1, "%s %s %s\r\n", Method, waisState->request,
	    waisState->request_hdr);
    else
	snprintf(buf, len + 1, "%s %s\r\n", Method, waisState->request);
    debug(24, 6) ("waisSendRequest: buf: %s\n", buf);
    comm_write(fd,
	buf,
	len,
	waisSendComplete,
	(void *) waisState,
	xfree);
    if (BIT_TEST(waisState->entry->flag, ENTRY_CACHABLE))
	storeSetPublicKey(waisState->entry);	/* Make it public */
}

void
waisStart(request_t * request, StoreEntry * entry)
{
    WaisStateData *waisState = NULL;
    int fd;
    char *url = entry->url;
    method_t method = request->method;
    debug(24, 3) ("waisStart: \"%s %s\"\n", RequestMethodStr[method], url);
    if (!Config.Wais.relayHost) {
	ErrorState *err;
	debug(24, 0) ("waisStart: Failed because no relay host defined!\n");
	/* was assert */
	err = xcalloc(1, sizeof(ErrorState));
	err->type = ERR_NO_RELAY;
	err->http_status = HTTP_INTERNAL_SERVER_ERROR;
	err->request = urlParse(METHOD_CONNECT, waisState->request);
	errorAppendEntry(entry, err);

	storeAbort(entry, 0);
	return;
    }
    fd = comm_open(SOCK_STREAM,
	0,
	Config.Addrs.tcp_outgoing,
	0,
	COMM_NONBLOCKING,
	url);
    if (fd == COMM_ERROR) {
	ErrorState *err;
	debug(24, 4) ("waisStart: Failed because we're out of sockets.\n");
	/* was assert */
	err = xcalloc(1, sizeof(ErrorState));
	err->type = ERR_SOCKET_FAILURE;
	err->http_status = HTTP_INTERNAL_SERVER_ERROR;
	err->request = urlParse(METHOD_CONNECT, waisState->request);
	errorAppendEntry(entry, err);
	storeAbort(entry, 0);
	return;
    }
    waisState = xcalloc(1, sizeof(WaisStateData));
    cbdataAdd(waisState);
    waisState->method = method;
    waisState->relayhost = Config.Wais.relayHost;
    waisState->relayport = Config.Wais.relayPort;
    waisState->request_hdr = request->headers;
    waisState->fd = fd;
    waisState->entry = entry;
    xstrncpy(waisState->request, url, MAX_URL);
    comm_add_close_handler(waisState->fd, waisStateFree, waisState);
    storeRegisterAbort(entry, waisAbort, waisState);
    commSetTimeout(fd, Config.Timeout.read, waisTimeout, waisState);
    storeLockObject(entry);
    commConnectStart(waisState->fd,
	waisState->relayhost,
	waisState->relayport,
	waisConnectDone,
	waisState);
}

static void
waisConnectDone(int fd, int status, void *data)
{
    WaisStateData *waisState = data;
    char *request = waisState->request;
    ErrorState *err;

    if (status == COMM_ERR_DNS) {
	/* was assert */
	err = xcalloc(1, sizeof(ErrorState));
	err->type = ERR_DNS_FAIL;
	err->http_status = HTTP_SERVICE_UNAVAILABLE;
	err->dnsserver_msg = xstrdup(dns_error_message);
	err->request = urlParse(METHOD_CONNECT, request);
	errorAppendEntry(waisState->entry, err);
	storeAbort(waisState->entry, 0);
	comm_close(fd);
    } else if (status != COMM_OK) {
	/* was assert */
	err = xcalloc(1, sizeof(ErrorState));
	err->type = ERR_CONNECT_FAIL;
	err->http_status = HTTP_SERVICE_UNAVAILABLE;
	err->errno = errno;
	err->host = xstrdup(waisState->relayhost);
	err->port = waisState->relayport;
	err->request = urlParse(METHOD_CONNECT, request);
	errorAppendEntry(waisState->entry, err);
	storeAbort(waisState->entry, 0);
	comm_close(fd);
    } else {
	commSetSelect(fd, COMM_SELECT_WRITE, waisSendRequest, waisState, 0);
    }
}

static void
waisAbort(void *data)
{
    HttpStateData *waisState = data;
    debug(24, 1) ("waisAbort: %s\n", waisState->entry->url);
    comm_close(waisState->fd);
}

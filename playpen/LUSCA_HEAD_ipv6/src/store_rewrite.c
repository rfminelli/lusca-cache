
/*
 * $Id$
 *
 * DEBUG: section 61    Redirector
 * AUTHOR: Duane Wessels
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

#include "squid.h"

typedef struct {
    void *data;
    char *orig_url;
    sqaddr_t client_addr2;
    const char *client_ident;
    const char *method_s;
    RH *handler;
} storeurlStateData;

static HLPCB storeurlHandleReply;
static void storeurlStateFree(storeurlStateData * r);
static helper *storeurlors = NULL;
static OBJH storeurlStats;
static int n_bypassed = 0;
CBDATA_TYPE(storeurlStateData);

static void
storeurlHandleReply(void *data, char *reply)
{
    storeurlStateData *r = data;
    int valid;
    char *t;
    debug(61, 5) ("storeurlHandleReply: {%s}\n", reply ? reply : "<NULL>");
    if (reply) {
	if ((t = strchr(reply, ' ')))
	    *t = '\0';
	if (*reply == '\0')
	    reply = NULL;
    }
    valid = cbdataValid(r->data);
    cbdataUnlock(r->data);
    if (valid)
	r->handler(r->data, reply);
    storeurlStateFree(r);
}

static void
storeurlStateFree(storeurlStateData * r)
{
    safe_free(r->orig_url);
    sqinet_done(&r->client_addr2);
    cbdataFree(r);
}

static void
storeurlStats(StoreEntry * sentry)
{
    storeAppendPrintf(sentry, "Redirector Statistics:\n");
    helperStats(sentry, storeurlors);
    if (Config.onoff.storeurl_bypass)
	storeAppendPrintf(sentry, "\nNumber of requests bypassed "
	    "because all store url bypassers were busy: %d\n", n_bypassed);
}

/**** PUBLIC FUNCTIONS ****/

void
storeurlStart(clientHttpRequest * http, RH * handler, void *data)
{
    ConnStateData *conn = http->conn;
    storeurlStateData *r = NULL;
    const char *fqdn;
    char *urlgroup = conn->port->urlgroup;
    char buf[8192];
    char claddr[MAX_IPSTRLEN];
    char myaddr[MAX_IPSTRLEN];
    assert(http);
    assert(handler);
    debug(61, 5) ("storeurlStart: '%s'\n", http->uri);
    if (Config.onoff.storeurl_bypass && storeurlors->stats.queue_size) {
	/* Skip storeurlor if there is one request queued */
	n_bypassed++;
	handler(data, NULL);
	return;
    }
    r = cbdataAlloc(storeurlStateData);
    r->orig_url = xstrdup(http->uri);
    sqinet_init(&r->client_addr2);
    sqinet_copy(&r->client_addr2, &conn->log_addr2);
    r->client_ident = NULL;
    if (http->request->auth_user_request)
	r->client_ident = authenticateUserRequestUsername(http->request->auth_user_request);
    else if (http->request->extacl_user) {
	r->client_ident = http->request->extacl_user;
    }
    if (!r->client_ident && conn->rfc931[0])
	r->client_ident = conn->rfc931;
#if USE_SSL
    if (!r->client_ident)
	r->client_ident = sslGetUserEmail(fd_table[conn->fd].ssl);
#endif
    if (!r->client_ident)
	r->client_ident = dash_str;
    r->method_s = urlMethodGetConstStr(http->request->method);
    r->handler = handler;
    r->data = data;
    cbdataLock(r->data);
    if ((fqdn = fqdncache_gethostbyaddr6(&r->client_addr2, 0)) == NULL)
	fqdn = dash_str;
    sqinet_ntoa(&r->client_addr2, claddr, sizeof(claddr), SQADDR_NONE);
    (void) sqinet_ntoa(&http->request->my_address, myaddr, sizeof(myaddr), SQADDR_NONE);
    snprintf(buf, 8191, "%s %s/%s %s %s %s myip=%s myport=%d",
	r->orig_url,
	claddr,
	fqdn,
	r->client_ident[0] ? rfc1738_escape(r->client_ident) : dash_str,
	r->method_s,
	urlgroup ? urlgroup : "-",
	myaddr,
	sqinet_get_port(&http->request->my_address));
    debug(61, 6) ("storeurlStart: sending '%s' to the helper\n", buf);
    strcat(buf, "\n");
    helperSubmit(storeurlors, buf, storeurlHandleReply, r);
}

void
storeurlInit(void)
{
    static int init = 0;
    if (!Config.Program.store_rewrite.command)
	return;
    if (storeurlors == NULL)
	storeurlors = helperCreate("store_rewriter");
    storeurlors->cmdline = Config.Program.store_rewrite.command;
    storeurlors->n_to_start = Config.Program.store_rewrite.children;
    storeurlors->concurrency = Config.Program.store_rewrite.concurrency;
    storeurlors->ipc_type = IPC_STREAM;
    helperOpenServers(storeurlors);
    if (!init) {
	cachemgrRegister("store_rewriter",
	    "URL Rewriter Stats",
	    storeurlStats, 0, 1);
	init = 1;
	CBDATA_INIT_TYPE(storeurlStateData);
    }
}

void
storeurlShutdown(void)
{
    if (!storeurlors)
	return;
    helperShutdown(storeurlors);
    if (!shutting_down)
	return;
    helperFree(storeurlors);
    storeurlors = NULL;
}

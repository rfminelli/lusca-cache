
/*
 * $Id$
 *
 * DEBUG: section 33    Client-side Routines
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

static void clientRedirectDone _PARAMS((void *data, char *result));
static void icpHandleIMSReply _PARAMS((int fd, void *data));
static void clientLookupDstIPDone _PARAMS((int fd, const ipcache_addrs *, void *data));
static void clientLookupSrcFQDNDone _PARAMS((int fd, const char *fqdn, void *data));
static void clientLookupDstFQDNDone _PARAMS((int fd, const char *fqdn, void *data));
static int clientGetsOldEntry _PARAMS((StoreEntry * new, StoreEntry * old, request_t * request));
static int checkAccelOnly _PARAMS((icpStateData * icpState));


static void
clientLookupDstIPDone(int fd, const ipcache_addrs * ia, void *data)
{
    icpStateData *icpState = data;
    icpState->ip_lookup_pending = 0;
    debug(33, 5, "clientLookupDstIPDone: FD %d, '%s'\n",
	fd,
	icpState->url);
    icpState->aclChecklist->state[ACL_DST_IP] = ACL_LOOKUP_DONE;
    if (ia) {
	icpState->aclChecklist->dst_addr = ia->in_addrs[0];
	debug(33, 5, "clientLookupDstIPDone: %s is %s\n",
	    icpState->request->host,
	    inet_ntoa(icpState->aclChecklist->dst_addr));
    }
    clientAccessCheck(icpState, icpState->aclHandler);
}

static void
clientLookupSrcFQDNDone(int fd, const char *fqdn, void *data)
{
    icpStateData *icpState = data;
    debug(33, 5, "clientLookupSrcFQDNDone: FD %d, '%s', FQDN %s\n",
	fd,
	icpState->url,
	fqdn ? fqdn : "NULL");
    icpState->aclChecklist->state[ACL_SRC_DOMAIN] = ACL_LOOKUP_DONE;
    clientAccessCheck(icpState, icpState->aclHandler);
}

static void
clientLookupDstFQDNDone(int fd, const char *fqdn, void *data)
{
    icpStateData *icpState = data;
    debug(33, 5, "clientLookupDstFQDNDone: FD %d, '%s', FQDN %s\n",
	fd,
	icpState->url,
	fqdn ? fqdn : "NULL");
    icpState->aclChecklist->state[ACL_SRC_DOMAIN] = ACL_LOOKUP_DONE;
    clientAccessCheck(icpState, icpState->aclHandler);
}

static void
clientLookupIdentDone(void *data)
{
    icpStateData *icpState = data;
    clientAccessCheck(icpState, icpState->aclHandler);
}

#if USE_PROXY_AUTH
/* ProxyAuth code by Jon Thackray <jrmt@uk.gdscorp.com> */
/* return 1 if allowed, 0 if denied */
static int
clientProxyAuthCheck(icpStateData * icpState)
{
    const char *proxy_user;

    /* Check that the user is allowed to access via this proxy-cache
     * don't restrict if they're accessing a local domain or
     * an object of type cacheobj:// */
    if (Config.proxyAuth.File == NULL)
	return 1;
    if (urlParseProtocol(icpState->url) == PROTO_CACHEOBJ)
	return 1;

    if (Config.proxyAuth.IgnoreDomains) {
	if (aclMatchRegex(Config.proxyAuth.IgnoreDomains, icpState->request->host)) {
	    debug(33, 2, "clientProxyAuthCheck: host \"%s\" matched proxyAuthIgnoreDomains\n", icpState->request->host);
	    return 1;
	}
    }
    proxy_user = proxyAuthenticate(icpState->request_hdr);
    xstrncpy(icpState->ident.ident, proxy_user, ICP_IDENT_SZ);
    debug(33, 6, "clientProxyAuthCheck: user = %s\n", icpState->ident.ident);

    if (strcmp(icpState->ident.ident, dash_str) == 0)
	return 0;
    return 1;
}
#endif /* USE_PROXY_AUTH */

static int
checkAccelOnly(icpStateData * icpState)
{
    /* return TRUE if someone makes a proxy request to us and
     * we are in httpd-accel only mode */
    if (!httpd_accel_mode)
	return 0;
    if (Config.Accel.withProxy)
	return 0;
    if (icpState->request->protocol == PROTO_CACHEOBJ)
	return 0;
    if (icpState->accel)
	return 0;
    return 1;
}

void
clientAccessCheck(icpStateData * icpState, void (*handler) (icpStateData *, int))
{
    int answer = 1;
    aclCheck_t *ch = NULL;
    char *browser = NULL;
    const ipcache_addrs *ia = NULL;

    if (Config.identLookup && icpState->ident.state == IDENT_NONE) {
	icpState->aclHandler = handler;
	identStart(-1, icpState, clientLookupIdentDone);
	return;
    }
    if (icpState->aclChecklist == NULL) {
	icpState->aclChecklist = xcalloc(1, sizeof(aclCheck_t));
	icpState->aclChecklist->src_addr = icpState->peer.sin_addr;
	icpState->aclChecklist->request = requestLink(icpState->request);
	browser = mime_get_header(icpState->request_hdr, "User-Agent");
	if (browser != NULL) {
	    xstrncpy(icpState->aclChecklist->browser, browser, BROWSERNAMELEN);
	} else {
	    icpState->aclChecklist->browser[0] = '\0';
	}
	xstrncpy(icpState->aclChecklist->ident,
	    icpState->ident.ident,
	    ICP_IDENT_SZ);
    }
    /* This so we can have SRC ACLs for cache_host_acl. */
    icpState->request->client_addr = icpState->peer.sin_addr;
#if USE_PROXY_AUTH
    if (clientProxyAuthCheck(icpState) == 0) {
	char *wbuf = NULL;
	int fd = icpState->fd;
	debug(33, 4, "Proxy Denied: %s\n", icpState->url);
	icpState->log_type = ERR_PROXY_DENIED;
	icpState->http_code = 407;
	wbuf = xstrdup(proxy_denied_msg(icpState->http_code,
		icpState->method,
		icpState->url,
		fd_table[fd].ipaddr));
	icpSendERROR(fd, icpState->log_type, wbuf, icpState, icpState->http_code);
	safe_free(icpState->aclChecklist);
	return;
    }
#endif /* USE_PROXY_AUTH */

    ch = icpState->aclChecklist;
    icpState->aclHandler = handler;
    if (checkAccelOnly(icpState)) {
	answer = 0;
    } else {
	answer = aclCheck(HTTPAccessList, ch);
	if (ch->state[ACL_DST_IP] == ACL_LOOKUP_NEED) {
	    ch->state[ACL_DST_IP] = ACL_LOOKUP_PENDING;		/* first */
	    icpState->ip_lookup_pending = 1;
	    ipcache_nbgethostbyname(icpState->request->host,
		icpState->fd,
		clientLookupDstIPDone,
		icpState);
	    return;
	} else if (ch->state[ACL_SRC_DOMAIN] == ACL_LOOKUP_NEED) {
	    ch->state[ACL_SRC_DOMAIN] = ACL_LOOKUP_PENDING;	/* first */
	    fqdncache_nbgethostbyaddr(icpState->peer.sin_addr,
		icpState->fd,
		clientLookupSrcFQDNDone,
		icpState);
	    return;
	} else if (ch->state[ACL_DST_DOMAIN] == ACL_LOOKUP_NEED) {
	    ch->state[ACL_DST_DOMAIN] = ACL_LOOKUP_PENDING;	/* first */
	    ia = ipcacheCheckNumeric(icpState->request->host);
	    if (ia != NULL)
		fqdncache_nbgethostbyaddr(ia->in_addrs[0],
		    icpState->fd,
		    clientLookupDstFQDNDone,
		    icpState);
	    return;
	}
    }
    requestUnlink(icpState->aclChecklist->request);
    safe_free(icpState->aclChecklist);
    icpState->aclHandler = NULL;
    handler(icpState, answer);
}

void
clientAccessCheckDone(icpStateData * icpState, int answer)
{
    int fd = icpState->fd;
    char *buf = NULL;
    char *redirectUrl = NULL;
    debug(33, 5, "clientAccessCheckDone: '%s' answer=%d\n", icpState->url, answer);
    if (answer) {
	urlCanonical(icpState->request, icpState->url);
	if (icpState->redirect_state != REDIRECT_NONE)
	    fatal_dump("clientAccessCheckDone: wrong redirect_state");
	icpState->redirect_state = REDIRECT_PENDING;
	redirectStart(fd, icpState, clientRedirectDone, icpState);
    } else {
	debug(33, 5, "Access Denied: %s\n", icpState->url);
	redirectUrl = aclGetDenyInfoUrl(&DenyInfoList, AclMatchedName);
	if (redirectUrl) {
	    icpState->http_code = 302,
		buf = access_denied_redirect(icpState->http_code,
		icpState->method,
		icpState->url,
		fd_table[fd].ipaddr,
		redirectUrl);
	} else {
	    icpState->http_code = 400;
	    buf = access_denied_msg(icpState->http_code,
		icpState->method,
		icpState->url,
		fd_table[fd].ipaddr);
	}
	icpSendERROR(fd, LOG_TCP_DENIED, buf, icpState, icpState->http_code);
    }
}

static void
clientRedirectDone(void *data, char *result)
{
    icpStateData *icpState = data;
    int fd = icpState->fd;
    int l;
    request_t *new_request = NULL;
    request_t *old_request = icpState->request;
    debug(33, 5, "clientRedirectDone: '%s' result=%s\n", icpState->url,
	result ? result : "NULL");
    if (icpState->redirect_state != REDIRECT_PENDING)
	fatal_dump("clientRedirectDone: wrong redirect_state");
    icpState->redirect_state = REDIRECT_DONE;
    if (result)
	new_request = urlParse(old_request->method, result);
    if (new_request) {
	safe_free(icpState->url);
	/* need to malloc because the URL returned by the redirector might
	 * not be big enough to append the local domain
	 * -- David Lamkin drl@net-tel.co.uk */
	l = strlen(result) + Config.appendDomainLen + 5;
	icpState->url = xcalloc(l, 1);
	xstrncpy(icpState->url, result, l);
	new_request->http_ver = old_request->http_ver;
	requestUnlink(old_request);
	icpState->request = requestLink(new_request);
	urlCanonical(icpState->request, icpState->url);
	safe_free(icpState->log_url);
	icpState->log_url = xstrdup(urlCanonicalClean(new_request));
    }
    icpParseRequestHeaders(icpState);
    fd_note(fd, icpState->log_url);
    if (!BIT_TEST(icpState->request->flags, REQ_PROXY_KEEPALIVE)) {
	commSetSelect(fd,
	    COMM_SELECT_READ,
	    icpDetectClientClose,
	    (void *) icpState,
	    0);
    }
    icpProcessRequest(fd, icpState);
}

#if USE_PROXY_AUTH
/* Check the modification time on the file that holds the proxy
 * passwords every 'n' seconds, and if it has changed, reload it
 */
#define CHECK_PROXY_FILE_TIME 300

const char *
proxyAuthenticate(const char *headers)
{
    /* Keep the time measurements and the hash
     * table of users and passwords handy */
    static time_t last_time = 0;
    static time_t change_time = 0;
    static HashID validated = 0;
    static char *passwords = NULL;
    LOCAL_ARRAY(char, sent_user, ICP_IDENT_SZ);

    char *s = NULL;
    char *sent_userandpw = NULL;
    char *user = NULL;
    char *passwd = NULL;
    char *clear_userandpw = NULL;
    struct stat buf;
    int i;
    hash_link *hashr = NULL;
    FILE *f = NULL;

    /* Look for Proxy-authorization: Basic in the
     * headers sent by the client
     */
    if ((s = mime_get_header(headers, "Proxy-authorization:")) == NULL) {
	debug(33, 5, "proxyAuthenticate: Can't find authorization header\n");
	return (dash_str);
    }
    /* Skip the 'Basic' part */
    s += strlen(" Basic");
    sent_userandpw = xstrdup(s);
    strtok(sent_userandpw, "\n");	/* Trim trailing \n before decoding */
    clear_userandpw = uudecode(sent_userandpw);
    xfree(sent_userandpw);

    xstrncpy(sent_user, clear_userandpw, ICP_IDENT_SZ);
    strtok(sent_user, ":");	/* Remove :password */
    debug(33, 5, "proxyAuthenticate: user = %s\n", sent_user);

    /* Look at the Last-modified time of the proxy.passwords
     * file every five minutes, to see if it's been changed via
     * a cgi-bin script, etc. If so, reload a fresh copy into memory
     */

    if ((squid_curtime - last_time) > CHECK_PROXY_FILE_TIME) {
	debug(33, 5, "proxyAuthenticate: checking password file %s hasn't changed\n", Config.proxyAuth.File);

	if (stat(Config.proxyAuth.File, &buf) == 0) {
	    if (buf.st_mtime != change_time) {
		debug(33, 0, "proxyAuthenticate: reloading changed proxy authentication password file %s \n", Config.proxyAuth.File);
		change_time = buf.st_mtime;

		if (validated != 0) {
		    debug(33, 5, "proxyAuthenticate: invalidating old entries\n");
		    for (i = 0, hashr = hash_first(validated); hashr; hashr = hash_next(validated)) {
			debug(33, 6, "proxyAuthenticate: deleting %s\n", hashr->key);
			hash_delete(validated, hashr->key);
		    }
		} else {
		    /* First time around, 7921 should be big enough */
		    if ((validated = hash_create(urlcmp, 7921, hash_string)) < 0) {
			debug(33, 1, "ERK: can't create hash table. Turning auth off");
			xfree(Config.proxyAuth.File);
			Config.proxyAuth.File = NULL;
			return (dash_str);
		    }
		}

		passwords = xmalloc((size_t) buf.st_size + 2);
		f = fopen(Config.proxyAuth.File, "r");
		fread(passwords, (size_t) buf.st_size, 1, f);
		*(passwords + buf.st_size) = '\0';
		strcat(passwords, "\n");
		fclose(f);

		user = strtok(passwords, ":");
		passwd = strtok(NULL, "\n");

		debug(33, 5, "proxyAuthenticate: adding new passwords to hash table\n");
		while (user != NULL) {
		    if (strlen(user) > 1 && passwd && strlen(passwd) > 1) {
			debug(33, 6, "proxyAuthenticate: adding %s, %s to hash table\n", user, passwd);
			hash_insert(validated, xstrdup(user), (void *) xstrdup(passwd));
		    }
		    user = strtok(NULL, ":");
		    passwd = strtok(NULL, "\n");
		}

		xfree(passwords);
	    }
	} else {
	    debug(33, 1, "ERK: can't access proxy_auth file %s. Turning authentication off", Config.proxyAuth.File);
	    xfree(Config.proxyAuth.File);
	    Config.proxyAuth.File = NULL;
	    return (dash_str);
	}
	last_time = squid_curtime;
    }
    hashr = hash_lookup(validated, sent_user);
    if (hashr == NULL) {
	/* User doesn't exist; deny them */
	debug(33, 4, "proxyAuthenticate: user %s doesn't exist\n", sent_user);
	xfree(clear_userandpw);
	return (dash_str);
    }
    passwd = strstr(clear_userandpw, ":");
    passwd++;

    /* See if we've already validated them */
    if (strcmp(hashr->item, passwd) == 0) {
	debug(33, 5, "proxyAuthenticate: user %s previously validated\n", sent_user);
	xfree(clear_userandpw);
	return sent_user;
    }
    if (strcmp(hashr->item, (char *) crypt(passwd, hashr->item))) {
	/* Passwords differ, deny access */
	debug(33, 4, "proxyAuthenticate: authentication failed: user %s passwords differ\n", sent_user);
	xfree(clear_userandpw);
	return (dash_str);
    }
    debug(33, 5, "proxyAuthenticate: user %s validated\n", sent_user);
    hash_delete(validated, sent_user);
    hash_insert(validated, xstrdup(sent_user), (void *) xstrdup(passwd));

    xfree(clear_userandpw);
    return (sent_user);
}
#endif /* USE_PROXY_AUTH */

void
icpProcessExpired(int fd, void *data)
{
    icpStateData *icpState = data;
    char *url = icpState->url;
    char *request_hdr = icpState->request_hdr;
    StoreEntry *entry = NULL;

    debug(33, 3, "icpProcessExpired: FD %d '%s'\n", fd, icpState->url);

    BIT_SET(icpState->request->flags, REQ_REFRESH);
    icpState->old.entry = icpState->entry;
    icpState->old.swapin_fd = icpState->swapin_fd;
    /* Create an entry for the IMS request; hopefully it comes back 304 */
    entry = storeCreateEntry(url,
	icpState->log_url,
	request_hdr,
	icpState->req_hdr_sz,
	icpState->request->flags,
	icpState->method);
    /* NOTE, don't call storeLockObject(), storeCreateEntry() does it */
    /* Tell store we're interested in both objects.  Otherwise they might
     * go into delete-behind mode */
    storeClientListAdd(entry, fd);
    storeClientListAdd(icpState->old.entry, fd);

    entry->lastmod = icpState->old.entry->lastmod;
    debug(33, 5, "icpProcessExpired: setting lmt = %d\n",
	entry->lastmod);

    entry->refcount++;		/* EXPIRED CASE */
    icpState->entry = entry;
    icpState->swapin_fd = storeOpenSwapFileRead(entry);
    if (icpState->swapin_fd < 0)
	fatal_dump("icpProcessExpired: Swapfile open failed");
    icpState->out.offset = 0;
    /* Register with storage manager to receive updates when data comes in. */
    storeRegister(entry, fd, icpHandleIMSReply, (void *) icpState, icpState->out.offset);
    protoDispatch(fd, url, icpState->entry, icpState->request);
}

static int
clientGetsOldEntry(StoreEntry * new_entry, StoreEntry * old_entry, request_t * request)
{
    /* If the reply is anything but "Not Modified" then
     * we must forward it to the client */
    if (new_entry->mem_obj->reply->code != 304) {
	debug(33, 5, "clientGetsOldEntry: NO, reply=%d\n", new_entry->mem_obj->reply->code);
	return 0;
    }
    /* If the client did not send IMS in the request, then it
     * must get the old object, not this "Not Modified" reply */
    if (!BIT_TEST(request->flags, REQ_IMS)) {
	debug(33, 5, "clientGetsOldEntry: YES, no client IMS\n");
	return 1;
    }
    /* If the client IMS time is prior to the entry LASTMOD time we
     * need to send the old object */
    if (modifiedSince(old_entry, request)) {
	debug(33, 5, "clientGetsOldEntry: YES, modified since %d\n", request->ims);
	return 1;
    }
    debug(33, 5, "clientGetsOldEntry: NO, new one is fine\n");
    return 0;
}



static void
icpHandleIMSReply(int fd, void *data)
{
    icpStateData *icpState = data;
    StoreEntry *entry = icpState->entry;
    MemObject *mem = entry->mem_obj;
    int unlink_request = 0;
    debug(33, 3, "icpHandleIMSReply: FD %d '%s'\n", fd, entry->key);
    if (entry->store_status == STORE_ABORTED) {
	debug(33, 3, "icpHandleIMSReply: ABORTED/%s '%s'\n",
	    log_tags[entry->mem_obj->abort_code], entry->url);
	/* We have an existing entry, but failed to validate it,
	 * so send the old one anyway */
	icpState->log_type = LOG_TCP_REFRESH_FAIL_HIT;
	storeUnregister(entry, fd);
	storeUnlockObject(entry);
	icpState->entry = icpState->old.entry;
	icpState->old.entry = NULL;
	if (icpState->swapin_fd > -1)
	    file_close(icpState->swapin_fd);
	icpState->swapin_fd = icpState->old.swapin_fd;
	icpState->old.swapin_fd = -1;
	icpState->entry->refcount++;
    } else if (mem->reply->code == 0) {
	debug(33, 3, "icpHandleIMSReply: Incomplete headers for '%s'\n",
	    entry->url);
	storeRegister(entry,
	    fd,
	    icpHandleIMSReply,
	    (void *) icpState,
	    entry->object_len);
	return;
    } else if (clientGetsOldEntry(entry, icpState->old.entry, icpState->request)) {
	/* We initiated the IMS request, the client is not expecting
	 * 304, so put the good one back. */
	icpState->log_type = LOG_TCP_REFRESH_HIT;
	if (icpState->old.entry->mem_obj->request == NULL) {
	    icpState->old.entry->mem_obj->request = requestLink(mem->request);
	    unlink_request = 1;
	}
	xmemcpy(icpState->old.entry->mem_obj->reply,
	    entry->mem_obj->reply,
	    sizeof(struct _http_reply));
	storeTimestampsSet(icpState->old.entry);
	storeUnregister(entry, fd);
	storeUnlockObject(entry);
	entry = icpState->entry = icpState->old.entry;
	icpState->old.entry = NULL;
	if (icpState->swapin_fd > -1)
	    file_close(icpState->swapin_fd);
	icpState->swapin_fd = icpState->old.swapin_fd;
	icpState->old.swapin_fd = -1;
	entry->timestamp = squid_curtime;
	if (unlink_request) {
	    requestUnlink(entry->mem_obj->request);
	    entry->mem_obj->request = NULL;
	}
    } else {
	/* the client can handle this reply, whatever it is */
	icpState->log_type = LOG_TCP_REFRESH_MISS;
	if (mem->reply->code == 304) {
	    icpState->old.entry->timestamp = squid_curtime;
	    icpState->old.entry->refcount++;
	    icpState->log_type = LOG_TCP_REFRESH_HIT;
	}
	storeUnregister(icpState->old.entry, fd);
	storeUnlockObject(icpState->old.entry);
	icpState->old.entry = NULL;
	if (icpState->old.swapin_fd > -1)
	    file_close(icpState->old.swapin_fd);
	icpState->old.swapin_fd = -1;
	if (icpState->swapin_fd < 0)
	    fatal_dump("icpHandleIMSReply: bad swapin_fd");
    }
    if (icpState->old.entry != NULL)
	debug_trap("old.entry still set");
    if (icpState->old.swapin_fd > -1)
	debug_trap("old.swapin_fd still set");
    storeRegister(icpState->entry,
	fd,
	icpSendMoreData,
	icpState,
	icpState->out.offset);
}

int
modifiedSince(StoreEntry * entry, request_t * request)
{
    int object_len;
    MemObject *mem = entry->mem_obj;
    debug(33, 3, "modifiedSince: '%s'\n", entry->key);
    if (entry->lastmod < 0)
	return 1;
    /* Find size of the object */
    if (mem->reply->content_length)
	object_len = mem->reply->content_length;
    else
	object_len = entry->object_len - mem->reply->hdr_sz;
    if (entry->lastmod > request->ims) {
	debug(33, 3, "--> YES: entry newer than client\n");
	return 1;
    } else if (entry->lastmod < request->ims) {
	debug(33, 3, "-->  NO: entry older than client\n");
	return 0;
    } else if (request->imslen < 0) {
	debug(33, 3, "-->  NO: same LMT, no client length\n");
	return 0;
    } else if (request->imslen == object_len) {
	debug(33, 3, "-->  NO: same LMT, same length\n");
	return 0;
    } else {
	debug(33, 3, "--> YES: same LMT, different length\n");
	return 1;
    }
}

char *
clientConstructTraceEcho(icpStateData * icpState)
{
    LOCAL_ARRAY(char, line, 256);
    LOCAL_ARRAY(char, buf, 8192);
    size_t len;
    memset(buf, '\0', 8192);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(line, "Date: %s\r\n", mkrfc1123(squid_curtime));
    strcat(buf, line);
    sprintf(line, "Server: Squid/%s\r\n", SQUID_VERSION);
    strcat(buf, line);
    sprintf(line, "Content-Type: message/http\r\n");
    strcat(buf, line);
    strcat(buf, "\r\n");
    len = strlen(buf);
    httpBuildRequestHeader(icpState->request,
	icpState->request,
	NULL,			/* entry */
	icpState->request_hdr,
	NULL,			/* in_len */
	buf + len,
	8192 - len,
	icpState->fd);
    icpState->log_type = LOG_TCP_MISS;
    icpState->http_code = 200;
    return buf;
}

void
clientPurgeRequest(icpStateData * icpState)
{
    char *buf;
    int fd = icpState->fd;
    LOCAL_ARRAY(char, msg, 8192);
    LOCAL_ARRAY(char, line, 256);
    StoreEntry *entry;
    debug(0, 0, "Config.Options.enable_purge = %d\n", Config.Options.enable_purge);
    if (!Config.Options.enable_purge) {
	buf = access_denied_msg(icpState->http_code = 401,
	    icpState->method,
	    icpState->url,
	    fd_table[fd].ipaddr);
	icpSendERROR(fd, LOG_TCP_DENIED, buf, icpState, icpState->http_code);
	return;
    }
    icpState->log_type = LOG_TCP_MISS;
    if ((entry = storeGet(icpState->url)) == NULL) {
	sprintf(msg, "HTTP/1.0 404 Not Found\r\n");
	icpState->http_code = 404;
    } else {
	storeRelease(entry);
	sprintf(msg, "HTTP/1.0 200 OK\r\n");
	icpState->http_code = 200;
    }
    sprintf(line, "Date: %s\r\n", mkrfc1123(squid_curtime));
    strcat(msg, line);
    sprintf(line, "Server: Squid/%s\r\n", SQUID_VERSION);
    strcat(msg, line);
    strcat(msg, "\r\n");
    comm_write(fd,
	msg,
	strlen(msg),
	30,
	icpSendERRORComplete,
	(void *) icpState,
	NULL);
}


/*
 * $Id$
 *
 * DEBUG: section 33    Client-side Routines
 * AUTHOR: Duane Wessels
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

#include "squid.h"

static void clientRedirectDone _PARAMS((void *data, char *result));
static int icpHandleIMSReply _PARAMS((int fd, StoreEntry * entry, void *data));


static int clientLookupDstIPDone(fd, hp, data)
     int fd;
     struct hostent *hp;
     void *data;
{
    icpStateData *icpState = data;
    debug(33, 5, "clientLookupDstIPDone: FD %d, '%s'\n",
	fd,
	icpState->url);
    icpState->aclChecklist->state[ACL_DST_IP] = ACL_LOOKUP_DONE;
    if (hp) {
	xmemcpy(&icpState->aclChecklist->dst_addr.s_addr,
	    *(hp->h_addr_list),
	    hp->h_length);
	debug(33, 5, "clientLookupDstIPDone: %s is %s\n",
	    icpState->request->host,
	    inet_ntoa(icpState->aclChecklist->dst_addr));
    }
    clientAccessCheck(icpState, icpState->aclHandler);
    return 1;
}

static void clientLookupSrcFQDNDone(fd, fqdn, data)
     int fd;
     char *fqdn;
     void *data;
{
    icpStateData *icpState = data;
    debug(33, 5, "clientLookupSrcFQDNDone: FD %d, '%s', FQDN %s\n",
	fd,
	icpState->url,
	fqdn ? fqdn : "NULL");
    icpState->aclChecklist->state[ACL_SRC_DOMAIN] = ACL_LOOKUP_DONE;
    clientAccessCheck(icpState, icpState->aclHandler);
}

#ifdef UNUSED_CODE
static void clientLookupIdentDone(data)
     void *data;
{
}

#endif

#if USE_PROXY_AUTH
/* return 1 if allowed, 0 if denied */
static int clientProxyAuthCheck(icpState)
     icpStateData *icpState;
{
    char *proxy_user;

    /* Check that the user is allowed to access via this proxy-cache
     * don't restrict if they're accessing a local domain or
     * an object of type cacheobj:// */
    if (Config.proxyAuthFile == NULL)
	return 1;
    if (urlParseProtocol(icpState->url) == PROTO_CACHEOBJ)
	return 1;
    if (Config.proxyAuthIgnoreDomain != NULL)
	if (matchDomainName(Config.proxyAuthIgnoreDomain, icpState->request->host))
	    return 1;
    proxy_user = proxyAuthenticate(icpState->request_hdr);
    strncpy(icpState->ident, proxy_user, ICP_IDENT_SZ);
    debug(33, 6, "jrmt: user = %s\n", icpState->ident);

    if (strcmp(icpState->ident, dash_str) == 0)
	return 0;
    return 1;
}
#endif /* USE_PROXY_AUTH */

void clientAccessCheck(icpState, handler)
     icpStateData *icpState;
     void (*handler) _PARAMS((icpStateData *, int));
{
    int answer = 1;
    request_t *r = icpState->request;
    aclCheck_t *ch = NULL;

    if (icpState->aclChecklist == NULL) {
	icpState->aclChecklist = xcalloc(1, sizeof(aclCheck_t));
	icpState->aclChecklist->src_addr = icpState->peer.sin_addr;
	icpState->aclChecklist->request = requestLink(icpState->request);
    }
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
    if (httpd_accel_mode && !Config.Accel.withProxy && r->protocol != PROTO_CACHEOBJ) {
	/* this cache is an httpd accelerator ONLY */
	if (!BIT_TEST(icpState->flags, REQ_ACCEL))
	    answer = 0;
    } else {
	answer = aclCheck(HTTPAccessList, ch);
	if (ch->state[ACL_DST_IP] == ACL_LOOKUP_NEED) {
	    ch->state[ACL_DST_IP] = ACL_LOOKUP_PENDING;		/* first */
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
	}
    }
    requestUnlink(icpState->aclChecklist->request);
    safe_free(icpState->aclChecklist);
    icpState->aclHandler = NULL;
    handler(icpState, answer);
}

void clientAccessCheckDone(icpState, answer)
     icpStateData *icpState;
     int answer;
{
    int fd = icpState->fd;
    char *buf = NULL;
    char *redirectUrl = NULL;
    debug(33, 5, "clientAccessCheckDone: '%s' answer=%d\n", icpState->url, answer);
    if (answer) {
	urlCanonical(icpState->request, icpState->url);
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

static void clientRedirectDone(data, result)
     void *data;
     char *result;
{
    icpStateData *icpState = data;
    int fd = icpState->fd;
    request_t *new_request = NULL;
    debug(33, 5, "clientRedirectDone: '%s' result=%s\n", icpState->url,
	result ? result : "NULL");
    if (result)
	new_request = urlParse(icpState->request->method, result);
    if (new_request) {
	safe_free(icpState->url);
	icpState->url = xstrdup(result);
	requestUnlink(icpState->request);
	icpState->request = requestLink(new_request);
	urlCanonical(icpState->request, icpState->url);
    }
    icpParseRequestHeaders(icpState);
    fd_note(fd, icpState->url);
    comm_set_select_handler(fd,
	COMM_SELECT_READ,
	(PF) icpDetectClientClose,
	(void *) icpState);
    icpProcessRequest(fd, icpState);
#if USE_PROXY_AUTH
}

/* Check the modification time on the file that holds the proxy
 * passwords every 'n' seconds, and if it has changed, reload it
 */
#define CHECK_PROXY_FILE_TIME 300

char *proxyAuthenticate(char *headers)
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
    time_t current_time;
    struct stat buf;
    int i;
    hash_link *hashr = NULL;
    FILE *f = NULL;

    /* Look for Proxy-authorization: Basic in the
     * headers sent by the client
     */
    if ((s = mime_get_header(headers, "Proxy-authorization:")) == NULL) {
	debug(33, 5, "jrmt: Can't find authorization header\n");
	return (dash_str);
    }
    /* Skip the 'Basic' part */
    s += strlen(" Basic");
    sent_userandpw = xstrdup(s);
    strtok(sent_userandpw, "\n");	/* Trim trailing \n before decoding */
    clear_userandpw = uudecode(sent_userandpw);
    xfree(sent_userandpw);

    strncpy(sent_user, clear_userandpw, ICP_IDENT_SZ);
    strtok(sent_user, ":");	/* Remove :password */
    debug(33, 5, "jrmt: user = %s\n", sent_user);

    /* Look at the Last-modified time of the proxy.passwords
     * file every five minutes, to see if it's been changed via
     * a cgi-bin script, etc. If so, reload a fresh copy into memory
     */

    current_time = time(NULL);

    if ((current_time - last_time) > CHECK_PROXY_FILE_TIME) {
	debug(33, 5, "jrmt: checking password file %s hasn't changed\n", Config.proxyAuthFile);

	if (stat(Config.proxyAuthFile, &buf) == 0) {
	    if (buf.st_mtime != change_time) {
		debug(33, 0, "jrmt: reloading changed proxy authentication password file %s \n", Config.proxyAuthFile);
		change_time = buf.st_mtime;

		if (validated != 0) {
		    debug(33, 5, "jrmt: invalidating old entries\n");
		    for (i = 0, hashr = hash_first(validated); hashr; hashr = hash_next(validated)) {
			debug(33, 6, "jrmt: deleting %s\n", hashr->key);
			hash_delete(validated, hashr->key);
		    }
		} else {
		    /* First time around, 7921 should be big enough */
		    if ((validated = hash_create(urlcmp, 7921, hash_string)) < 0) {
			debug(1, 1, "ERK: can't create hash table. Turning auth off");
			xfree(Config.proxyAuthFile);
			Config.proxyAuthFile = NULL;
			return (dash_str);
		    }
		}

		passwords = xmalloc((size_t) buf.st_size + 2);
		f = fopen(Config.proxyAuthFile, "r");
		fread(passwords, buf.st_size, 1, f);
		*(passwords + buf.st_size) = '\0';
		strcat(passwords, "\n");
		fclose(f);

		user = strtok(passwords, ":");
		passwd = strtok(NULL, "\n");

		debug(33, 5, "jrmt: adding new passwords to hash table\n");
		while (user != NULL) {
		    if (strlen(user) > 1 && strlen(passwd) > 1) {
			debug(33, 6, "jrmt: adding %s, %s to hash table\n", user, passwd);
			hash_insert(validated, xstrdup(user), (void *) xstrdup(passwd));
		    }
		    user = strtok(NULL, ":");
		    passwd = strtok(NULL, "\n");
		}

		xfree(passwords);
	    }
	} else {
	    debug(1, 1, "ERK: can't access proxy_auth file %s. Turning authentication off", Config.proxyAuthFile);
	    xfree(Config.proxyAuthFile);
	    Config.proxyAuthFile = NULL;
	    return (dash_str);
	}
    }
    last_time = current_time;

    hashr = hash_lookup(validated, sent_user);
    if (hashr == NULL) {
	/* User doesn't exist; deny them */
	debug(33, 4, "jrmt: user %s doesn't exist\n", sent_user);
	xfree(clear_userandpw);
	return (dash_str);
    }
    passwd = strstr(clear_userandpw, ":");
    passwd++;

    /* See if we've already validated them */
    if (strcmp(hashr->item, passwd) == 0) {
	debug(33, 5, "jrmt: user %s previously validated\n", sent_user);
	xfree(clear_userandpw);
	return sent_user;
    }
    if (strcmp(hashr->item, (char *) crypt(passwd, hashr->item))) {
	/* Passwords differ, deny access */
	debug(33, 4, "jrmt: authentication failed: user %s passwords differ\n", sent_user);
	xfree(clear_userandpw);
	return (dash_str);
    }
    debug(33, 5, "jrmt: user %s validated\n", sent_user);
    hash_delete(validated, sent_user);
    hash_insert(validated, xstrdup(sent_user), (void *) xstrdup(passwd));

    xfree(clear_userandpw);
    return (sent_user);
#endif /* USE_PROXY_AUTH */
}

int icpProcessExpired(fd, icpState)
     int fd;
     icpStateData *icpState;
{
    char *url = icpState->url;
    char *request_hdr = icpState->request_hdr;
    StoreEntry *entry = NULL;

    debug(33, 3, "icpProcessExpired: FD %d '%s'\n", fd, icpState->url);

    icpState->old_entry = icpState->entry;
    entry = storeCreateEntry(url,
	request_hdr,
	icpState->flags,
	icpState->method);
    /* NOTE, don't call storeLockObject(), storeCreateEntry() does it */

    entry->lastmod = icpState->old_entry->lastmod;
    debug(33, 5, "icpProcessExpired: setting lmt = %d\n",
	entry->lastmod);

    entry->refcount++;		/* MISS CASE */
    entry->mem_obj->fd_of_first_client = fd;
    icpState->entry = entry;
    icpState->offset = 0;
    /* Register with storage manager to receive updates when data comes in. */
    storeRegister(entry, fd, (PIF) icpHandleIMSReply, (void *) icpState);
    return (protoDispatch(fd, url, icpState->entry, icpState->request));
}


int icpHandleIMSReply(fd, entry, data)
     int fd;
     StoreEntry *entry;
     void *data;
{
    icpStateData *icpState = data;
    MemObject *mem = entry->mem_obj;
    LOCAL_ARRAY(char, hbuf, 8192);
    int len;
    int unlink_request = 0;
    debug(33, 0, "icpHandleIMSReply: FD %d '%s'\n", fd, entry->url);
    /* unregister this handler */
    storeUnregister(entry, fd);
    if (entry->store_status == STORE_ABORTED) {
	debug(33, 0, "icpHandleIMSReply: abort_code=%d\n",
	    entry->mem_obj->abort_code);
	icpSendERROR(fd,
	    entry->mem_obj->abort_code,
	    entry->mem_obj->e_abort_msg,
	    icpState,
	    400);
	return 0;
    }
    if (mem->reply->code == 304 && !BIT_TEST(icpState->flags, REQ_IMS)) {
	icpState->log_type = LOG_TCP_EXPIRED_HIT;
	/* We initiated the IMS request, the client is not expecting
	 * 304, so put the good one back */
	if (icpState->old_entry->mem_obj->request == NULL) {
	    icpState->old_entry->mem_obj->request = requestLink(mem->request);
	    unlink_request = 1;
	}
	storeUnlockObject(entry);
	entry = icpState->entry = icpState->old_entry;
	/* Extend the TTL
	 * XXX race condition here.  Assumes old_entry has been swapped 
	 * in by the time this 304 reply arrives.  */
	storeClientCopy(entry, 0, 8191, hbuf, &len, fd);
	if (!mime_headers_end(hbuf))
	    fatal_dump("icpHandleIMSReply: failed to load headers, lost race");
	httpParseHeaders(hbuf, entry->mem_obj->reply);
	ttlSet(entry);
	if (unlink_request) {
	    requestUnlink(entry->mem_obj->request);
	    entry->mem_obj->request = NULL;
	}
    } else {
	/* the client can handle this reply, whatever it is */
	icpState->log_type = LOG_TCP_EXPIRED_MISS;
	storeUnlockObject(icpState->old_entry);
    }
    icpState->old_entry = NULL;		/* done with old_entry */
    icpSendMoreData(fd, icpState);	/* give data to the client */
    return 1;
}

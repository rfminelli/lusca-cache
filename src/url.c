
/*
 * $Id$
 *
 * DEBUG: section 23    URL Parsing
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

const char *RequestMethodStr[] =
{
    "NONE",
    "GET",
    "POST",
    "PUT",
    "HEAD",
    "CONNECT",
    "TRACE",
    "PURGE"
};

const char *ProtocolStr[] =
{
    "NONE",
    "http",
    "ftp",
    "gopher",
    "wais",
    "cache_object",
    "icp",
    "urn",
    "whois",
    "internal",
    "TOTAL"
};

static int url_ok[256];
static const char *const hex = "0123456789abcdef";
static request_t *urnParse(method_t method, char *urn);
static const char *const valid_hostname_chars =
#if ALLOW_HOSTNAME_UNDERSCORES
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789-._";
#else
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789-.";
#endif
static const char *const valid_url_chars =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./-_$";

/* convert %xx in url string to a character 
 * Allocate a new string and return a pointer to converted string */

char *
url_convert_hex(char *org_url, int allocate)
{
    static char code[] = "00";
    char *url = NULL;
    char *s = NULL;
    char *t = NULL;
    url = allocate ? (char *) xstrdup(org_url) : org_url;
    if ((int) strlen(url) < 3 || !strchr(url, '%'))
	return url;
    for (s = t = url; *(s + 2); s++) {
	if (*s == '%') {
	    code[0] = *(++s);
	    code[1] = *(++s);
	    *t++ = (char) strtol(code, NULL, 16);
	} else {
	    *t++ = *s;
	}
    }
    do {
	*t++ = *s;
    } while (*s++);
    return url;
}

void
urlInitialize(void)
{
    int i;
    debug(23, 5) ("urlInitialize: Initializing...\n");
    assert(sizeof(ProtocolStr) == (PROTO_MAX + 1) * sizeof(char *));
    for (i = 0; i < 256; i++)
	url_ok[i] = strchr(valid_url_chars, (char) i) ? 1 : 0;

}

/* Encode prohibited char in string */
/* return the pointer to new (allocated) string */
char *
url_escape(const char *url)
{
    const char *p;
    char *q;
    char *tmpline = xcalloc(1, MAX_URL);
    q = tmpline;
    for (p = url; *p; p++) {
	if (url_ok[(int) (*p)])
	    *q++ = *p;
	else {
	    *q++ = '%';		/* Means hex coming */
	    *q++ = hex[(int) ((*p) >> 4)];
	    *q++ = hex[(int) ((*p) & 15)];
	}
    }
    *q++ = '\0';
    return tmpline;
}

method_t
urlParseMethod(const char *s)
{
    if (strcasecmp(s, "GET") == 0) {
	return METHOD_GET;
    } else if (strcasecmp(s, "POST") == 0) {
	return METHOD_POST;
    } else if (strcasecmp(s, "PUT") == 0) {
	return METHOD_PUT;
    } else if (strcasecmp(s, "HEAD") == 0) {
	return METHOD_HEAD;
    } else if (strcasecmp(s, "CONNECT") == 0) {
	return METHOD_CONNECT;
    } else if (strcasecmp(s, "TRACE") == 0) {
	return METHOD_TRACE;
    } else if (strcasecmp(s, "PURGE") == 0) {
	return METHOD_PURGE;
    }
    return METHOD_NONE;
}


protocol_t
urlParseProtocol(const char *s)
{
    if (strncasecmp(s, "http", 4) == 0)
	return PROTO_HTTP;
    if (strncasecmp(s, "ftp", 3) == 0)
	return PROTO_FTP;
    if (strncasecmp(s, "file", 4) == 0)
	return PROTO_FTP;
    if (strncasecmp(s, "gopher", 6) == 0)
	return PROTO_GOPHER;
    if (strncasecmp(s, "wais", 4) == 0)
	return PROTO_WAIS;
    if (strncasecmp(s, "cache_object", 12) == 0)
	return PROTO_CACHEOBJ;
    if (strncasecmp(s, "urn", 3) == 0)
	return PROTO_URN;
    if (strncasecmp(s, "whois", 5) == 0)
	return PROTO_WHOIS;
    if (strncasecmp(s, "internal", 8) == 0)
	return PROTO_INTERNAL;
    return PROTO_NONE;
}


int
urlDefaultPort(protocol_t p)
{
    switch (p) {
    case PROTO_HTTP:
	return 80;
    case PROTO_FTP:
	return 21;
    case PROTO_GOPHER:
	return 70;
    case PROTO_WAIS:
	return 210;
    case PROTO_CACHEOBJ:
    case PROTO_INTERNAL:
	return CACHE_HTTP_PORT;
    case PROTO_WHOIS:
	return 43;
    default:
	return 0;
    }
}

request_t *
urlParse(method_t method, char *url)
{
    LOCAL_ARRAY(char, proto, MAX_URL);
    LOCAL_ARRAY(char, login, MAX_URL);
    LOCAL_ARRAY(char, host, MAX_URL);
    LOCAL_ARRAY(char, urlpath, MAX_URL);
    request_t *request = NULL;
    char *t = NULL;
    int port;
    protocol_t protocol = PROTO_NONE;
    int l;
    proto[0] = host[0] = urlpath[0] = login[0] = '\0';

    if ((l = strlen(url)) + Config.appendDomainLen > (MAX_URL - 1)) {
	/* terminate so it doesn't overflow other buffers */
	*(url + (MAX_URL >> 1)) = '\0';
	debug(23, 0) ("urlParse: URL too large (%d bytes)\n", l);
	return NULL;
    }
    if (method == METHOD_CONNECT) {
	port = CONNECT_PORT;
	if (sscanf(url, "%[^:]:%d", host, &port) < 1)
	    return NULL;
    } else if (!strncmp(url, "urn:", 4)) {
	return urnParse(method, url);
    } else {
	if (sscanf(url, "%[^:]://%[^/]%s", proto, host, urlpath) < 2)
	    return NULL;
	protocol = urlParseProtocol(proto);
	port = urlDefaultPort(protocol);
	/* Is there any login informaiton? */
	if ((t = strrchr(host, '@'))) {
	    strcpy(login, host);
	    t = strrchr(login, '@');
	    *t = 0;
	    strcpy(host, t + 1);
	}
	if ((t = strrchr(host, ':'))) {
	    *t++ = '\0';
	    if (*t != '\0')
		port = atoi(t);
	}
    }
    for (t = host; *t; t++)
	*t = tolower(*t);
    if (strspn(host, valid_hostname_chars) != strlen(host)) {
	debug(23, 1) ("urlParse: Illegal character in hostname '%s'\n", host);
	return NULL;
    }
    /* remove trailing dots from hostnames */
    while ((l = strlen(host)) > 0 && host[--l] == '.')
	host[l] = '\0';
    if (Config.appendDomain && !strchr(host, '.'))
	strncat(host, Config.appendDomain, SQUIDHOSTNAMELEN);
    if (port == 0) {
	debug(23, 0) ("urlParse: Invalid port == 0\n");
	return NULL;
    }
#ifdef HARDCODE_DENY_PORTS
    /* These ports are filtered in the default squid.conf, but
     * maybe someone wants them hardcoded... */
    if (port == 7 || port == 9 || port = 19) {
	debug(23, 0) ("urlParse: Deny access to port %d\n", port);
	return NULL;
    }
#endif
    request = memAllocate(MEM_REQUEST_T);
    request->method = method;
    request->protocol = protocol;
    xstrncpy(request->host, host, SQUIDHOSTNAMELEN);
    xstrncpy(request->login, login, MAX_LOGIN_SZ);
    request->port = (u_short) port;
#if OLD_CODE
    xstrncpy(request->urlpath, urlpath, MAX_URL);
#else
    stringReset(&request->urlpath, urlpath);
#endif
    request->max_age = -1;
    request->max_forwards = -1;
    return request;
}

static request_t *
urnParse(method_t method, char *urn)
{
    request_t *request = NULL;
    debug(50, 5) ("urnParse: %s\n", urn);
    request = memAllocate(MEM_REQUEST_T);
    request->method = method;
    request->protocol = PROTO_URN;
#if OLD_CODE
    xstrncpy(request->urlpath, &urn[4], MAX_URL);
#else
    stringReset(&request->urlpath, urn + 4);
#endif
    request->max_age = -1;
    request->max_forwards = -1;
    return request;
}

char *
urlCanonical(const request_t * request, char *buf)
{
    LOCAL_ARRAY(char, urlbuf, MAX_URL);
    LOCAL_ARRAY(char, portbuf, 32);
    if (buf == NULL)
	buf = urlbuf;
    if (request->protocol == PROTO_URN) {
	snprintf(buf, MAX_URL, "urn:%s", strBuf(request->urlpath));
    } else
	switch (request->method) {
	case METHOD_CONNECT:
	    snprintf(buf, MAX_URL, "%s:%d", request->host, request->port);
	    break;
	default:
	    portbuf[0] = '\0';
	    if (request->port != urlDefaultPort(request->protocol))
		snprintf(portbuf, 32, ":%d", request->port);
	    snprintf(buf, MAX_URL, "%s://%s%s%s%s%s",
		ProtocolStr[request->protocol],
		request->login,
		*request->login ? "@" : null_string,
		request->host,
		portbuf,
		strBuf(request->urlpath));
	    break;
	}
    return buf;
}

char *
urlCanonicalClean(const request_t * request)
{
    LOCAL_ARRAY(char, buf, MAX_URL);
    LOCAL_ARRAY(char, portbuf, 32);
    LOCAL_ARRAY(char, loginbuf, MAX_LOGIN_SZ + 1);
    char *t;
    if (request->protocol == PROTO_URN) {
	snprintf(buf, MAX_URL, "urn:%s", strBuf(request->urlpath));
    } else
	switch (request->method) {
	case METHOD_CONNECT:
	    snprintf(buf, MAX_URL, "%s:%d", request->host, request->port);
	    break;
	default:
	    portbuf[0] = '\0';
	    if (request->port != urlDefaultPort(request->protocol))
		snprintf(portbuf, 32, ":%d", request->port);
	    loginbuf[0] = '\0';
	    if ((int) strlen(request->login) > 0) {
		strcpy(loginbuf, request->login);
		if ((t = strchr(loginbuf, ':')))
		    *t = '\0';
		strcat(loginbuf, "@");
	    }
	    snprintf(buf, MAX_URL, "%s://%s%s%s%s",
		ProtocolStr[request->protocol],
		loginbuf,
		request->host,
		portbuf,
		strBuf(request->urlpath));
	    if ((t = strchr(buf, '?')))
		*t = '\0';
	    break;
	}
    return buf;
}

request_t *
requestLink(request_t * request)
{
    request->link_count++;
    return request;
}

void
requestUnlink(request_t * request)
{
    if (request == NULL)
	return;
    request->link_count--;
    if (request->link_count)
	return;
    safe_free(request->headers);
    safe_free(request->body);
    stringClean(&request->urlpath);
    memFree(MEM_REQUEST_T, request);
}

int
matchDomainName(const char *domain, const char *host)
{
    int offset;
    if ((offset = strlen(host) - strlen(domain)) < 0)
	return 0;		/* host too short */
    if (strcasecmp(domain, host + offset) != 0)
	return 0;		/* no match at all */
    if (*domain == '.')
	return 1;
    if (*(host + offset - 1) == '.')
	return 1;
    if (offset == 0)
	return 1;
    return 0;
}

int
urlCheckRequest(const request_t * r)
{
    int rc = 0;
    if (r->method == METHOD_CONNECT)
	return 1;
    if (r->method == METHOD_TRACE)
	return 1;
    if (r->method == METHOD_PURGE)
	return 1;
    switch (r->protocol) {
    case PROTO_URN:
    case PROTO_HTTP:
    case PROTO_CACHEOBJ:
	rc = 1;
	break;
    case PROTO_FTP:
	if (r->method == METHOD_PUT)
	    rc = 1;
    case PROTO_GOPHER:
    case PROTO_WAIS:
    case PROTO_WHOIS:
	if (r->method == METHOD_GET)
	    rc = 1;
	else if (r->method == METHOD_HEAD)
	    rc = 1;
	break;
    default:
	break;
    }
    return rc;
}

/*
 * Quick-n-dirty host extraction from a URL.  Steps:
 *	Look for a colon
 *	Skip any '/' after the colon
 *	Copy the next SQUID_MAXHOSTNAMELEN bytes to host[]
 *	Look for an ending '/' or ':' and terminate
 *	Look for login info preceeded by '@'
 */
char *
urlHostname(const char *url)
{
    LOCAL_ARRAY(char, host, SQUIDHOSTNAMELEN);
    char *t;
    host[0] = '\0';
    if (NULL == (t = strchr(url, ':')))
	return NULL;
    t++;
    while (*t != '\0' && *t == '/')
	t++;
    xstrncpy(host, t, SQUIDHOSTNAMELEN);
    if ((t = strchr(host, '/')))
	*t = '\0';
    if ((t = strchr(host, ':')))
	*t = '\0';
    if ((t = strrchr(host, '@'))) {
	t++;
        xmemmove(host, t, strlen(t)+1);
    }
    return host;
}

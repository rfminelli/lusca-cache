/*
 * $Id$
 *
 * AUTHOR: Alex Rousskov
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

#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_

#include "HttpMsg.h" /* @?@ -> structs.h */

/* generic http message (common portion of http Request and Reply) */
struct _HttpRequest {
    /* common fields with http reply (hack) */
#include "HttpMsgHack.h"

    /* public, readable (be careful: may be NULL) */

    /* results of parsing the HTTP Request-Line */
    const char *uri;
    const char *host;
    const char *login;
    const char *urlpath;
    const method_t method;
    const protocol_t protocol;
    const u_short port;        

    int flags;

    int accel;          /* true if in acceleration mode. @?@ move to flags? */

    HttpReply *reply;

    /* can be modified even for "const" requests */
    int link_count;     /* free when zero */

    struct in_addr client_addr;
    struct _HierarchyLogEntry hier;

    aclCheck_t *acl_checklist;        /* need ptr back so we can unreg if needed @?@ */

    log_type log_type;

#if 0
    /* info from http request-line */
    method_t method;
    protocol_t protocol;
    char login[MAX_LOGIN_SZ]; /* 128 */
    char host[SQUIDHOSTNAMELEN + 1]; /* 128 + 1 */
    char urlpath[MAX_URL]; /* 4096! */

    char *uri;
    float http_ver;
    aclCheck_t *acl_checklist;  /* need ptr back so we can unreg if needed */
    HttpRequest *next;          /* used in store only */
    

    /* logging stuff */
    struct _AccessLogEntry al;
    char *log_uri;
    struct timeval start;

    /* protected, do not use these, use interface functions instead */
#endif
};

/* create/destroy */
extern HttpRequest *httpRequestCreate();
extern void httpRequestDestroy(HttpMsg *msg);
extern HttpRequest *httpRequestClone(HttpRequest *orig);

/* set pre-parsed uri info */
extern void httpRequestSetUri(HttpRequest *, method_t, protocol_t, const char *host, u_short port, const char *login, const char *urlpath);

int httpRequestIsConditional(HttpRequest *req);

#endif /* ndef _HTTP_REQUEST_H_ */

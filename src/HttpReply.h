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

#ifndef _HTTP_REPLY_H_
#define _HTTP_REPLY_H_

/* generic http message (common portion of http Request and Reply) */
struct _HttpReply {
    /* common fields with http reply (hack) */
#include "HttpMsgHack.h"

    HttpRequest *request;

    /* public, readable (be careful: may be NULL/undefined) */
    err_type err_type;
    AccessLogEntry al;

#if 0
    /* info from http status-line */
    http_code code;
    char *uri;
    float http_ver;
    aclCheck_t *acl_checklist;  /* need ptr back so we can unreg if needed */
    HttpReply *next;            /* used in store only */
    
    /* logging stuff */
    struct _AccessLogEntry al;
    char *log_uri;
    log_type log_type;
    struct timeval start;

    /* protected, do not use these, use interface functions instead */
#endif
};

/* create/destroy */
extern HttpReply *httpReplyCreate();
extern void httpReplyDestroy(HttpReply *req);

#endif /* ndef _HTTP_REPLY_H_ */

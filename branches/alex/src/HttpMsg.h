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

#ifndef _HTTP_MSG_H_
#define _HTTP_MSG_H_

/* generic http message (common portion of http Request and Reply)
struct _HttpMsg {
    /* public, writable (using corresponding interfaces) */
    HttpHeader *header;
    HttpConn *conn;    /* the connection this came from */

    /* protected, do not use these, use interface functions instead */
    IOBuffer *buf;     /* comm | buf | store */
    StoreEntry *body;  /* body (if any) is always passed via store */
};

typedef struct _HttpMsg HttpMsg;

/* create/destroy */
extern HttpMsg *httpMsgCreate();
extern void httpMsgDestroy(HttpMsg *msg);

/* parses a message initializing headers and such */
extern int httpMsgParse(HttpMsg *msg, const char *buf);

/* puts report on current header usage and other stats into a static string */
extern const char *httpMsgReport();

#endif /* ndef _HTTP_MSG_H_ */

/*
 * $Id$
 *
 * AUTHOR:
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

#ifndef _HTTP_CONN_H_
#define _HTTP_CONN_H_

#include "IOBuffer.h" /* @?@ -> structs.h */


struct _HttpConn {
    IOBuffer in_buf;
    IOBuffer out_buf;

    int fd;

    HttpMsg *reader;        /* current reader or NULL */
    HttpMsg *writer;        /* current writer or NULL */

    HttpMsg *(*get_reader)(HttpConn *conn); /* called when new reader is needed */
    HttpMsg *(*get_writer)(HttpConn *conn); /* called when new writer is needed */

    int cc_level;           /* concurrency level: #concurrent xactions being processed */
    int req_count;          /* number of requests created or admitted */
    int rep_count;          /* number of replies processed */

#if 0
    HttpConnIndex index;    /* keeps pending writers, searches by HttpMsg->id */
    HttpConnDIndex deps;    /* keeps all HttpMsgs that depend on this connection */
#endif

    u_char timeout_count;   /* number of timeouts caught */

    char *host;             /* peer host */ /* yes, these duplicate .addr below @?@ */
    u_char port;            /* peer port */

    /* these are used by passive connections only @?@ */
    struct {
	struct sockaddr_in peer;
	struct sockaddr_in me;
	struct in_addr log;
    } addr;
    IdentStateData ident;

    /* these are used by active connections only @?@ */
    peer *peer;
};


/* Create a passive http connection */
extern HttpConn *httpConnAccept(int sock);

/* manage dependent */
void httpConnAddDep(HttpConn *conn, HttpMsg *dep);
void httpConnDelDep(HttpConn *conn, HttpMsg *dep);
void httpConnNoteReaderDone(HttpConn *conn, HttpMsg *msg);

/* entry points */
void httpConnSendReply(HttpConn *conn, HttpReply *rep);
void httpConnSendRequest(HttpConn *conn, HttpRequest *req);

#endif /* _HTTP_CONN_H_ */

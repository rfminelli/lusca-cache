/*
 * $Id$
 *
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

#ifndef ICP_H
#define ICP_H

typedef enum {
    LOG_TAG_NONE,		/* 0 */
    LOG_TCP_HIT,		/* 1 */
    LOG_TCP_MISS,		/* 2 */
    LOG_TCP_EXPIRED_HIT,	/* 3 */
    LOG_TCP_EXPIRED_MISS,	/* 5 */
    LOG_TCP_USER_REFRESH,	/* 6 */
    LOG_TCP_IFMODSINCE,		/* 6 */
    LOG_TCP_SWAPIN_FAIL,	/* 7 */
    LOG_TCP_DENIED,		/* 8 */
    LOG_UDP_HIT,		/* 9 */
    LOG_UDP_HIT_OBJ,		/* 10 */
    LOG_UDP_MISS,		/* 11 */
    LOG_UDP_DENIED,		/* 12 */
    LOG_UDP_INVALID,		/* 13 */
    LOG_UDP_RELOADING,		/* 14 */
    ERR_READ_TIMEOUT,		/* 15 */
    ERR_LIFETIME_EXP,		/* 16 */
    ERR_NO_CLIENTS_BIG_OBJ,	/* 17 */
    ERR_READ_ERROR,		/* 18 */
    ERR_CLIENT_ABORT,		/* 19 */
    ERR_CONNECT_FAIL,		/* 20 */
    ERR_INVALID_REQ,		/* 21 */
    ERR_UNSUP_REQ,		/* 22 */
    ERR_INVALID_URL,		/* 23 */
    ERR_NO_FDS,			/* 24 */
    ERR_DNS_FAIL,		/* 25 */
    ERR_NOT_IMPLEMENTED,	/* 26 */
    ERR_CANNOT_FETCH,		/* 27 */
    ERR_NO_RELAY,		/* 28 */
    ERR_DISK_IO,		/* 29 */
    ERR_ZERO_SIZE_OBJECT,	/* 30 */
    ERR_PROXY_DENIED		/* 31 */
} log_type;

#define ERR_MIN ERR_READ_TIMEOUT
#define ERR_MAX ERR_PROXY_DENIED

/* bitfields for the icpStateData 'flags' element */
#define		REQ_HTML	0x01
#define		REQ_NOCACHE	0x02
#define		REQ_IMS		0x04
#define		REQ_AUTH	0x08
#define		REQ_CACHABLE	0x10
#define 	REQ_ACCEL	0x20
#define 	REQ_HIERARCHICAL 0x40
#define 	REQ_LOOPDETECT  0x80

typedef struct wwd {
    struct sockaddr_in address;
    char *msg;
    long len;
    struct wwd *next;
    struct timeval start;
    log_type logcode;
} icpUdpData;

#define ICP_IDENT_SZ 63
typedef struct iwd {
    icp_common_t header;	/* for UDP_HIT_OBJ's */
    int fd;
    char *url;
    char *inbuf;
    int inbufsize;
    method_t method;		/* GET, POST, ... */
    request_t *request;		/* Parsed URL ... */
    char *request_hdr;		/* Mime header */
    StoreEntry *entry;
    StoreEntry *old_entry;
    long offset;
    int log_type;
    int http_code;
    struct sockaddr_in peer;
    struct sockaddr_in me;
    struct in_addr log_addr;
    char *buf;
    struct timeval start;
    int flags;
    int size;			/* hack for CONNECT which doesnt use sentry */
    char ident[ICP_IDENT_SZ + 1];
    int ident_fd;
    aclCheck_t *aclChecklist;
    void (*aclHandler) _PARAMS((struct iwd *, int answer));
    float http_ver;
} icpStateData;

extern int icpUdpSend _PARAMS((int,
	char *,
	icp_common_t *,
	struct sockaddr_in *,
	int flags,
	icp_opcode,
	log_type));
extern int icpHandleUdp _PARAMS((int sock, void *data));
extern int asciiHandleConn _PARAMS((int sock, void *data));
extern int icpSendERROR _PARAMS((int fd,
	log_type errorCode,
	char *text,
	icpStateData *,
	int httpCode));
extern void AppendUdp _PARAMS((icpUdpData *));
extern void icpParseRequestHeaders _PARAMS((icpStateData *));
extern void icpDetectClientClose _PARAMS((int fd, icpStateData *));
extern void icpProcessRequest _PARAMS((int fd, icpStateData *));
extern void icpHandleStore _PARAMS((int, StoreEntry *, icpStateData *));
extern int icpSendMoreData _PARAMS((int fd, icpStateData *));

extern int neighbors_do_private_keys;
extern char *IcpOpcodeStr[];
extern int icpUdpReply _PARAMS((int fd, icpUdpData * queue));

#endif /* ICP_H */


/*
 * $Id$
 *
 * DEBUG: section 17    Neighbor Selection
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

static int protoNotImplemented _PARAMS((int, const char *, StoreEntry *));

char *IcpOpcodeStr[] =
{
    "ICP_INVALID",
    "ICP_QUERY",
    "ICP_HIT",
    "ICP_MISS",
    "ICP_ERR",
    "ICP_SEND",
    "ICP_SENDA",
    "ICP_DATABEG",
    "ICP_DATA",
    "ICP_DATAEND",
    "ICP_SECHO",
    "ICP_DECHO",
    "ICP_OP_UNUSED0",
    "ICP_OP_UNUSED1",
    "ICP_OP_UNUSED2",
    "ICP_OP_UNUSED3",
    "ICP_OP_UNUSED4",
    "ICP_OP_UNUSED5",
    "ICP_OP_UNUSED6",
    "ICP_OP_UNUSED7",
    "ICP_OP_UNUSED8",
    "ICP_RELOADING",		/* access denied while store is reloading */
    "ICP_DENIED",
    "ICP_HIT_OBJ",
    "ICP_END"
};

void
protoDispatch(int fd, StoreEntry * entry, request_t * request)
{
    debug(17, 3, "protoDispatch: '%s'\n", entry->url);
    entry->mem_obj->request = requestLink(request);
    if (request->protocol == PROTO_CACHEOBJ) {
	protoStart(fd, entry, NULL, request);
	return;
    }
    if (request->protocol == PROTO_WAIS) {
	protoStart(fd, entry, NULL, request);
	return;
    }
    peerSelect(fd, request, entry);
}

int
protoUnregister(int fd, StoreEntry * entry, request_t * request, struct in_addr src_addr)
{
    char *url = entry ? entry->url : NULL;
    char *host = request ? request->host : NULL;
    protocol_t proto = request ? request->protocol : PROTO_NONE;
    debug(17, 5, "protoUnregister FD %d '%s'\n", fd, url ? url : "NULL");
    if (proto == PROTO_CACHEOBJ)
	return 0;
    if (url)
	redirectUnregister(url, fd);
    if (src_addr.s_addr != inaddr_none)
	fqdncacheUnregister(src_addr, fd);
    if (host)
	ipcache_unregister(host, fd);
    if (entry == NULL)
	return 0;
    if (BIT_TEST(entry->flag, ENTRY_DISPATCHED))
	return 0;
    if (entry->mem_status != NOT_IN_MEMORY)
	return 0;
    if (entry->store_status != STORE_PENDING)
	return 0;
    protoCancelTimeout(fd, entry);
    squid_error_entry(entry, ERR_CLIENT_ABORT, NULL);
    return 1;
}

void
protoCancelTimeout(int fd, StoreEntry * entry)
{
    /* If fd = 0 then this thread was called from neighborsUdpAck and
     * we must look up the FD in the pending list. */
    if (!fd)
	fd = storeFirstClientFD(entry->mem_obj);
    if (fd < 1) {
	debug(17, 1, "protoCancelTimeout: No client for '%s'\n", entry->url);
	return;
    }
    debug(17, 2, "protoCancelTimeout: FD %d '%s'\n", fd, entry->url);
    if (fdstatGetType(fd) != FD_SOCKET) {
	debug_trap("protoCancelTimeout: called on non-socket");
	return;
    }
    /* cancel the timeout handler */
    commSetSelect(fd,
	COMM_SELECT_TIMEOUT,
	NULL,
	NULL,
	0);
}

int
protoStart(int fd, StoreEntry * entry, peer * e, request_t * request)
{
    char *url = entry->url;
    char *request_hdr = entry->mem_obj->mime_hdr;
    int request_hdr_sz = entry->mem_obj->mime_hdr_sz;
    debug(17, 5, "protoStart: FD %d: Fetching '%s %s' from %s\n",
	fd,
	RequestMethodStr[request->method],
	entry->url,
	e ? e->host : "source");
    if (BIT_TEST(entry->flag, ENTRY_DISPATCHED))
	fatal_dump("protoStart: object already being fetched");
    BIT_SET(entry->flag, ENTRY_DISPATCHED);
    protoCancelTimeout(fd, entry);
    netdbPingSite(request->host);
    if (e) {
	e->stats.fetches++;
	return proxyhttpStart(url, request, entry, e);
    } else if (request->protocol == PROTO_HTTP) {
	return httpStart(url, request, request_hdr, request_hdr_sz, entry);
    } else if (request->protocol == PROTO_GOPHER) {
	return gopherStart(fd, url, entry);
    } else if (request->protocol == PROTO_FTP) {
	return ftpStart(fd, url, request, entry);
    } else if (request->protocol == PROTO_WAIS) {
	return waisStart(fd, url, request->method, request_hdr, entry);
    } else if (request->protocol == PROTO_CACHEOBJ) {
	return objcacheStart(fd, url, entry);
    } else if (request->method == METHOD_CONNECT) {
	fatal_dump("protoStart() should not be handling CONNECT");
	return 0;
    } else {
	return protoNotImplemented(fd, url, entry);
    }
    /* NOTREACHED */
}


static int
protoNotImplemented(int fd, const char *url, StoreEntry * entry)
{
    LOCAL_ARRAY(char, buf, 256);

    debug(17, 1, "protoNotImplemented: Cannot retrieve '%s'\n", url);

    buf[0] = '\0';
    if (httpd_accel_mode)
	strcpy(buf, "Squid is running in HTTPD accelerator mode, so it does not allow the normal URL syntax.");
    else
	sprintf(buf, "Your URL may be incorrect: '%s'\n", url);

    squid_error_entry(entry, ERR_NOT_IMPLEMENTED, NULL);
    return 0;
}

/*
 * $Id$
 *
 * DEBUG: section 54   Cache Engine
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

#include "squid.h"

/* Local constants */

/* Local functions */


/* entry point for _all_ requests that must be processed */
void
engineProcessRequest(Request *req)
{
    const cache_key *key;
    StoreEntry *e;

   assert(req);
   debug(54, 4) ("engineProcessRequest: %s '%s'\n",
	RequestMethodStr[req->method], req->uri);
   req->log_type = LOG_TCP_MISS; /* default */
   /* branch off depending on http method */
   switch (req->method) {
	case METHOD_CONNECT:
	    engineProcessConnectReq(req);
	    return;
	case METHOD_PURGE:
	    engineProcessPurgeReq(req);
	    return;
        case METHOD_TRACE:
	    engineProcessTraceReq(req);
	    return;
	case METHOD_NONE:
	    assert(0);
	default:
	    engineProcessStdReq(req);
	    return;
   }
   /* not reached */
}


static void
engineProcessConnectReq(Request *req)
{
    int fd = req->conn->fd;
    assert(0); /* check last parameter to sslStart! (was http->out_size) @?@ */
    sslStart(fd, req->uri, r, NULL);
}

static void
engineProcessPurgeReq(Request *req)
{
    assert(0); /* implement this ! @?@ */
}

static void
engineProcessTraceReq(Request *req)
{
    Reply *rep;
    if (r->max_forwards > 0) {
	engineProcessStdReq(req);
	return;
    }
    /* we are the last hop, reply back with our info */
    /* note that we need to send the original request back in our body */
    engineProcessReply(httpReplyCreateTrace(req));
}

static void
engineProcessStdRequest(Request *req)
{
    const cache_key *key = storeKeyPublic(req->uri, req->method);
    StoreEntry *e = storeGet(key);

    /* determine request type and double check entry */
    req->log_type = engineGetRequestType(req, &e);
    req->entry = e; /* does e belong to req or reply??? */
    debug(54, 4) ("engineProcessStdRequest: %s for '%s'\n",
	log_tags[req->log_type], req->uri);
    /* @?@ removed storeLockObject, storeCreateMemObject, storeClientListAdd */
    /* @?@ check were to put all entry->refcount++; */
    switch (req->log_type) {
	case LOG_TCP_HIT:
	case LOG_TCP_NEGATIVE_HIT:
	case LOG_TCP_MEM_HIT:
	   engineProcessHit(req);
	   break;
        case LOG_TCP_IMS_MISS:
           assert(0); /* .. wait for dw to answer this one .. */
           break;
	case LOG_TCP_REFRESH_MISS:
	   engineProcessStale(req);
	   break;
	default:
	   engineProcessMiss(req);
	   break;
    }
}


@?@ Where the hell is security stuff ??


/*
 * Determines what kind of hit or miss we have. This essential function might
 * look simple, but its logic is actually quite complex.
 */
static log_type
engineGetRequestType(Request *req, StoreEntry *pe) {
    StoreEntry *e = *pe;

    /* @?@ do we need to check for req->method == METHOD_PUT or METHOD_POST here?? */
    if (!e) {
	/* this object isn't in the cache */
	return = LOG_TCP_MISS;
    } else
    if (EBIT_TEST(e->flag, ENTRY_SPECIAL)) { 
	/* @?@ icons? */
	return (e->mem_status == IN_MEMORY) ? LOG_TCP_MEM_HIT : LOG_TCP_HIT;
    } else 
    if (!storeEntryValidToSend(e)) { 
	/* already released object, expired negative cache, etc. */
	storeRelease(e); /* get rid of invalid entry */
	*pe = 0;
	return LOG_TCP_MISS;
    } else
    if (EBIT_TEST(r->flags, REQ_NOCACHE)) {
	/* reply should not come from the cache */
	/* @?@ @?@ (check specs) we have to purge current entry unless request is conditional */
	if (!EBIT_TEST(r->flags, REQ_CONDL))
	    storeRelease(e);
	ipcacheReleaseInvalid(r->host);
	*pe = 0;
	return LOG_TCP_CLIENT_REFRESH;
    } else
    if (engineCheckNegativeHit(e)) {
	return LOG_TCP_NEGATIVE_HIT;
    } else
    if (engineCheckStale(e, r, 0)) {
	/*
	 * The object is in the cache, but is may be stale and needs to be
	 * validated.  Use LOG_TCP_REFRESH_MISS for the time being, maybe change
	 * it to _HIT if our copy is still fresh. If we have no way of
	 * validating freshness, return LOG_TCP_MISS.
	 */
	return (engineCanValidate(e, r)) ? LOG_TCP_REFRESH_MISS : LOG_TCP_MISS;
    } else
    if (EBIT_TEST(r->flags, REQ_IMS)) {
	/*
	 * If we got here, we think the object is fresh, but client wants to
	 * verify that. Use LOG_TCP_REFRESH_MISS for the time being, maybe
	 * change it to _HIT if verification succeeds
	 */
	return LOG_TCP_IMS_MISS;
    }
    /* we checked everything, it is a sure hit! */
    return (e->mem_status == IN_MEMORY) ? LOG_TCP_MEM_HIT : LOG_TCP_HIT;
}

/* attempts to find the requested object on the network */
static void
engineProcessMiss(Request *req) 
{
    /* req is a client request; we may want change it before sending */
    Request *ourReq = engineMakeForwardReq(req);
    serverSendRequest(ourReq);
}

/* modify client request to forward it further */
static void
engineMakeForwardReq(Request *req) 
{
    /* req is a client request; we may want change it before sending */
    Request *clone = httpRequestClone(req);
    /* remove headers we do not want */
    /* remove Connection: and "paranoid" stuff */
    /* double check what has to be removed here @?@ */
    /* httpHeaderCleanConnect(bad name)(&clone->header); */
}

/* entry point for _all_ replies that must be processed */
void
engineProcessReply(Reply *rep)
{
    assert(rep);
    assert(rep->request);
    assert(0); /* not implemented yet */
}

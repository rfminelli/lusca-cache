/*
 * $Id$
 *
 * DEBUG: section ??    HTTP Reply
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

#include "squid.h"
#include "HttpReply.h"
#include "HttpRequest.h"


/* local constants */

/* local routines */
static int httpReplyParseStart(HttpMsg *msg, const char *blk_start, const char *blk_end);
static void httpReplySetRState(HttpMsg *msg, ReadState rstate);
static void httpReplyNoteError(HttpMsg *msg, HttpReply *error);
static int httpStatusLineParse(HttpReply *rep, const char *blk_start, const char *blk_end);

HttpReply *
httpReplyCreate(HttpRequest *req)
{
    HttpReply *rep = memAllocate(MEM_HTTPREPLY, 1);
    httpMsgInit((HttpMsg*)rep);
    rep->request = req;
    /* custom methods */
    rep->parseStart = &httpReplyParseStart;
    rep->setRState = &httpReplySetRState;
    rep->noteError = &httpReplyNoteError;
    rep->destroy = &httpReplyDestroy;
    /* did we set everything? */
    httpMsgCheck((HttpMsg*)rep);
    return rep;
}

void
httpReplyDestroy(HttpMsg *msg)
{
    assert(msg);
    httpMsgClean(msg);
    memFree(MEM_HTTPREPLY, msg);
}


/*
 * parses http "command line", returns true on success
 */
static int
httpReplyParseStart(HttpMsg *msg, const char *blk_start, const char *blk_end)
{
    HttpReply *rep = (HttpReply*) msg;
    assert(rep->rstate == rsReadyToParse); /* the only state we can be called in */
    return httpStatusLineParse(rep, blk_start, blk_end);
}


static void
httpReplySetRState(HttpMsg *msg, ReadState rstate)
{
    httpMsgSetRState(msg, rstate); /* call parent first */
    if (msg->rstate == rsParsedHeaders)
	engineProcessReply((HttpReply*)msg); /* we are ready to be processed */
}

static void
httpReplyNoteError(HttpMsg *msg, HttpReply *error)
{
    /* replace our content with the error message @?@ */
    /* HttpReply *rep = (HttpReply*) msg; */
    assert(0); /* not implemented yet */
}

void
httpReplyNoteReqError(HttpReply *rep, HttpRequest *req)
{
    /* do this for now @?@ */
    assert(0); 
    /* httpReplyNoteError((HttpMsg*)rep, error); */
}


/*
 * routines to create standard replies
 */

HttpReply *
httpReplyCreateTrace(HttpRequest *req) {
    HttpReply *rep;
    assert(req);

    rep = httpReplyCreate(req);
    /* set headers */
    httpReplySetStatus(rep, 1.0, HTTP_OK);
    httpHeaderAddStrField(&rep->header, "Date", mkrfc1123(squid_curtime));
    httpHeaderAddStrField(&rep->header, "Server", full_appname_string);
    httpHeaderAddStrField(&rep->header, "Content-Type", "message/http");
    /* HTTP says we need to reply with original request as our body */
    assert(rep->entry != NULL);
    assert(req->entry != NULL);
    rep->entry = req->entry;
    storeClientListAdd(rep->entry, rep);
    /*
     * allocate buffer for incoming data and pass it to store_client.c; @?@ note
     * that ideally we would like to allocate buffer right before read, not in
     * advance; also would be much better to use ioBuffer
     */
    storeClientCopy(rep->entry, 0, 0, 4096, memAllocate(MEM_4K_BUF, 1), httpMsgNoteDataReady, rep);
    return rep;
}

static int
httpStatusLineParse(HttpReply *rep, const char *blk_start, const char *blk_end) {
    /* @?@ implement it */
    assert(0);
    return 0;
}

/*
 * $Id$
 *
 * DEBUG: section ??    HTTP Request
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
#include "HttpRequest.h"
#include "HttpReply.h"


/* local constants */

/* local routines */
static int httpRequestParseStart(HttpMsg *msg, const char *blk_start, const char *blk_end);
static void httpRequestSetRState(HttpMsg *msg, ReadState rstate);
static void httpRequestNoteError(HttpMsg *msg, HttpReply *error);


HttpRequest *
httpRequestCreate()
{
    HttpRequest *req = memAllocate(MEM_HTTPREQUEST, 1);
    httpMsgInit((HttpMsg*)req);
    /* custom methods */
    req->parseStart = &httpRequestParseStart;
    req->setRState = &httpRequestSetRState;
    req->noteError = &httpRequestNoteError;
    req->destroy = httpRequestDestroy;
    /* did we set everything? */
    httpMsgCheck((HttpMsg*)req);
    return req;
}

HttpRequest *
httpRequestClone(HttpRequest *orig)
{
    HttpRequest *clone = memAllocate(MEM_HTTPREQUEST, 1);
    httpMsgClone((HttpMsg*)orig, (HttpMsg*)clone);
    /* did we set everything? */
    httpMsgCheck((HttpMsg*)clone);
    return clone;
}

void
httpRequestDestroy(HttpMsg *msg)
{
    HttpRequest *req = (HttpRequest*)msg;
    assert(req);
    httpMsgClean((HttpMsg*)req);
    xfree((char*)req->host);
    xfree((char*)req->login);
    xfree((char*)req->urlpath);
    memFree(MEM_HTTPREQUEST, req);
}

/* set pre-parsed uri info */
void
httpRequestSetUri(HttpRequest *req,
    method_t method, protocol_t protocol, const char *host, 
    u_short port, const char *login, const char *urlpath)
{
    assert(req);
    assert(urlpath);
    req->method = method;
    req->protocol = protocol;
    req->host = xstrdup(host ? host : "");
    req->port = port;
    req->login = xstrdup(login ? login : "");
    req->urlpath = xstrdup(urlpath);
}

/*
 * parses http "request-line", returns true on success
 */
static int
httpRequestParseStart(HttpMsg *msg, const char *blk_start, const char *blk_end)
{
    HttpRequest *req = (HttpRequest*) msg;
    assert(req->rstate == rsReadyToParse); /* the only state we can be called in */
    assert(0); /* implement it here @?@ */
    return 0;
}


static void
httpRequestSetRState(HttpMsg *msg, ReadState rstate)
{
    httpMsgSetRState(msg, rstate); /* call parent first */
    if (msg->rstate == rsParsedHeaders)
	engineProcessRequest((HttpRequest*)msg); /* we are ready to be processed */
}

static void
httpRequestNoteError(HttpMsg *msg, HttpReply *error)
{
    HttpRequest *req = (HttpRequest*) msg;
    if (req->reply) { /* what to do if we already have a reply? @?@ */
	httpReplyNoteReqError(error, req);   
        httpReplyDestroy((HttpMsg*)error);
    } else {
	req->reply = error;
    }
}

int
httpRequestIsConditional(HttpRequest *req)
{
    static const int CondMask = 
	EBIT_MAKE(REQ_IMS) | 
	EBIT_MAKE(REQ_IMSR);

    assert(req);
    return req->flags & CondMask;
}

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


/* local constants */

/* local routines */
#if 0
static int httpReplyParseStart(HttpMsg *msg, const char *blk_start, const char *blk_end);
static void httpReplySetRState(HttpMsg *msg, ReadState rstate);
static void httpReplyNoteError(HttpMsg *msg, HttpReply *error);
static int httpStatusLineParse(HttpReply *rep, const char *blk_start, const char *blk_end);
#endif
static char *httpStatusString(http_status status);


/*
 * HTTP Reply
 */
HttpReply *
httpReplyCreate()
{
    HttpReply *rep = memAllocate(MEM_HTTPREPLY, 1);
    /* init with invalid values */
    rep->date = -2;
    rep->expires = -2;
    rep->last_modified = -2;
    rep->content_length = -1;
    return rep;
}

void
httpReplyDestroy(HttpReply *rep)
{
    assert(rep);
    memFree(MEM_HTTPREPLY, rep);
}

void
httpResponseSetHeaders(HttpResponse *resp, 
    double ver, http_status status,
    char *ctype, int clen, time_t lmt, time_t expires)
{
    httpStatusLineSet(&resp->sline, ver, status, httpStatusString(status));
    httpHeaderAddStr(&resp->hdr, "Server", full_appname_string);
    httpHeaderAddStr(&resp->hdr, "MIME-Version", "1.0"); /* was not set here before @?@ */
    httpHeaderAddDate(&resp->hdr, "Date", squid_curtime);
    if (clen > 0)
	httpHeaderAddInt(&resp->hdr, "Content-Length", clen);
    if (expires >= 0)
	httpHeaderAddDate(&resp->hdr, "Expires", expires);
    if (lmt > 0) /* this used to be lmt != 0 @?@ */
	httpHeaderAddDate(&resp->hdr, "Last-Modified", lmt);
    if (ctype)
	httpHeaderAddStr(&resp->hdr, "Content-Type", ctype);
}

void
httpStatusLineSwap(HttpStatusLine *sline, StoreEntry *e) {    
#if 0
    LOCAL_ARRAY(char, buf, HTTP_REPLY_BUF_SZ);
    int l = 0;
    int s = HTTP_REPLY_BUF_SZ;
    l += snprintf(buf + l, s - l, "HTTP/%3.1f %d %s\r\n",
	ver,
	(int) status,
	httpStatusString(status));
    l += snprintf(buf + l, s - l, "Server: Squid/%s\r\n", SQUID_VERSION);
    l += snprintf(buf + l, s - l, "Date: %s\r\n", mkrfc1123(squid_curtime));
    if (expires >= 0)
	l += snprintf(buf + l, s - l, "Expires: %s\r\n", mkrfc1123(expires));
    if (lmt)
	l += snprintf(buf + l, s - l, "Last-Modified: %s\r\n", mkrfc1123(lmt));
    if (clen > 0)
	l += snprintf(buf + l, s - l, "Content-Length: %d\r\n", clen);
    if (ctype)
	l += snprintf(buf + l, s - l, "Content-Type: %s\r\n", ctype);
    return buf;
#endif
}

#if 0

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

#endif

void
httpReplyUpdateOnNotModified(HttpReply *rep, HttpReply *freshRep)
{
    rep->cache_control = freshRep->cache_control;
    rep->misc_headers = freshRep->misc_headers;
    if (freshRep->date > -1)
	rep->date = freshRep->date;
    if (freshRep->last_modified > -1)
	rep->last_modified = freshRep->last_modified;
    if (freshRep->expires > -1)
	rep->expires = freshRep->expires;
}

static char *
httpStatusString(http_status status)
{
    /* why not to return matching string instead of using "p" ? @?@ */
    char *p = NULL;
    switch (status) {
    case 100:
	p = "Continue";
	break;
    case 101:
	p = "Switching Protocols";
	break;
    case 200:
	p = "OK";
	break;
    case 201:
	p = "Created";
	break;
    case 202:
	p = "Accepted";
	break;
    case 203:
	p = "Non-Authoritative Information";
	break;
    case 204:
	p = "No Content";
	break;
    case 205:
	p = "Reset Content";
	break;
    case 206:
	p = "Partial Content";
	break;
    case 300:
	p = "Multiple Choices";
	break;
    case 301:
	p = "Moved Permanently";
	break;
    case 302:
	p = "Moved Temporarily";
	break;
    case 303:
	p = "See Other";
	break;
    case 304:
	p = "Not Modified";
	break;
    case 305:
	p = "Use Proxy";
	break;
    case 400:
	p = "Bad Request";
	break;
    case 401:
	p = "Unauthorized";
	break;
    case 402:
	p = "Payment Required";
	break;
    case 403:
	p = "Forbidden";
	break;
    case 404:
	p = "Not Found";
	break;
    case 405:
	p = "Method Not Allowed";
	break;
    case 406:
	p = "Not Acceptable";
	break;
    case 407:
	p = "Proxy Authentication Required";
	break;
    case 408:
	p = "Request Time-out";
	break;
    case 409:
	p = "Conflict";
	break;
    case 410:
	p = "Gone";
	break;
    case 411:
	p = "Length Required";
	break;
    case 412:
	p = "Precondition Failed";
	break;
    case 413:
	p = "Request Entity Too Large";
	break;
    case 414:
	p = "Request-URI Too Large";
	break;
    case 415:
	p = "Unsupported Media Type";
	break;
    case 500:
	p = "Internal Server Error";
	break;
    case 501:
	p = "Not Implemented";
	break;
    case 502:
	p = "Bad Gateway";
	break;
    case 503:
	p = "Service Unavailable";
	break;
    case 504:
	p = "Gateway Time-out";
	break;
    case 505:
	p = "HTTP Version not supported";
	break;
    default:
	p = "Unknown";
	debug(11, 0) ("Unknown HTTP status code: %d\n", status);
	break;
    }
    return p;
}

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

#ifndef _HTTP_RESPONSE_H_
#define _HTTP_RESPONSE_H_

typedef struct _http_reply HttpReply;

/* http-status line */
struct _HttpStatusLine {
    double version;
    const char *reason;
    int code;
};

typedef struct _HttpStatusLine HttpStatusLine;

/* http-response message */
struct _HttpResponse {
    HttpStatusLine sline;
    HttpHeader hdr;
};

typedef struct _HttpResponse HttpResponse;

/*
 * HTTP Reply
 */

/* create/destroy */
extern HttpReply *httpReplyCreate();
extern void httpReplyDestroy(HttpReply *rep);

/* updatre when 34 reply is received for a cached object */
extern void httpReplyUpdateOnNotModified(HttpReply *rep, HttpReply *freshRep);

/*
 * HTTP Response
 */

/* create/init/clean/destroy */
extern HttpResponse *httpResponseCreate();
extern void httpResponseInit(HttpResponse *resp);
extern void httpResponseClean(HttpResponse *resp);
extern void httpResponseDestroy(HttpResponse *resp);

/* set commonly used info with one call */
extern void httpResponseSetHeaders(HttpResponse *resp, double ver, http_status status, char *ctype, int clen, time_t lmt, time_t expires);

/* parse/swap/summ */
/* parse a 0-terminating buffer and fill internal structires; returns true if successful */
extern int httpResponseParseHeaders(HttpResponse *resp, const char *resp_start);
/* swaps fields to store using storeAppendPrintf */
extern void httpResponseSwap(HttpResponse *resp, StoreEntry *e);
/* summarizes response in a compact HttpReply structure */
extern void httpResponseSumm(HttpResponse *resp, HttpReply *summ);

#if 0
/* called when corresponding request aborts */
void httpResponseNoteReqError(HttpResponse *rep, HttpRequest *req);

/* standard replies */
HttpResponse *httpResponseCreateTrace(HttpRequest *req);
#endif

/*
 * HTTP Status-Line
 */

/* sets http reply status line (first line in a reply) */
/* init/clean */
extern void httpStatusLineInit(HttpStatusLine *sline);
extern void httpStatusLineClean(HttpStatusLine *sline);

/* set values */
extern void httpStatusLineSet(HttpStatusLine *sline, float httpVer, http_status status, const char *reason);

/* parse/swap */
/* parse a 0-terminating buffer and fill internal structires; returns true if successful */
extern int httpStatusLineParse(HttpStatusLine *sline, const char *resp_start);
/* swaps fields to store using storeAppendPrintf */
extern void httpStatusLineSwap(HttpStatusLine *sline, StoreEntry *e);


#endif /* ifndef _HTTP_REPLY_H_ */

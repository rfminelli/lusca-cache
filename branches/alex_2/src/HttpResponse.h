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

/* status line */
struct _HttpStatusLine {
    /* public, read only */
    double version;
    const char *reason;
    http_status status;
    int packed_size;
};

typedef struct _HttpStatusLine HttpStatusLine;

/* body (content) */
/*
 * Note: Body is used only for replies with a small text content that is known a
 * priory (e.g., error messages).
 */
struct _HttpBody {
    /* public, read only */
    int packed_size;
    /* private, never dereference it */
    char *buf;     /* null terminating _text_ buffer, not for binary stuff */
};

typedef struct _HttpBody HttpBody;

typedef enum { psReadyToParseStartLine = 0, psReadyToParseHeaders, psParsed, psError } HttpMsgParseState;

/* http-response message */
struct _HttpResponse {
    /* public, writable */
    HttpStatusLine sline;
    HttpHeader hdr;
    HttpBody body; /* may be empty, se comments for HttpBody */

    /* public, readable */
    HttpMsgParseState pstate;
};

typedef struct _HttpResponse HttpResponse;

/*
 * HTTP Reply (summary of HttpResponse)
 */

/* create/destroy */
extern HttpReply *httpReplyCreate();
extern void httpReplyDestroy(HttpReply *rep);

/* updatre when 304 reply is received for a cached object */
extern void httpReplyUpdateOnNotModified(HttpReply *rep, HttpReply *freshRep);

/* parse */
extern int httpReplyParseHeaders(HttpReply *rep, const char *buf, const char **end);

/*
 * HTTP Response
 */

/* create/init/clean/destroy */
extern HttpResponse *httpResponseCreate();
extern void httpResponseInit(HttpResponse *resp);
extern void httpResponseClean(HttpResponse *resp);
extern void httpResponseDestroy(HttpResponse *resp);

/* set commonly used info with one call */
extern void httpResponseSetHeaders(HttpResponse *resp, double ver, http_status status, const char *reason, const char *ctype, int clen, time_t lmt, time_t expires);

/* parse/summ */
/* parse a 0-terminating buffer and fill internal structires; returns +1 (ok), 0 (more), or -1 (err) */
int httpResponseParse(HttpResponse *resp, const char *parse_start, const char * *parse_end_ptr);
/* summarizes response in a compact HttpReply structure */
extern void httpResponseSumm(HttpResponse *resp, HttpReply *summ);

/* swap/pack */
/* swaps fields to store using storeAppend */
extern void httpResponseSwap(HttpResponse *resp, StoreEntry *e);
/* creates a 4K, 8K, or bigger buffer and packs into it */
extern char *httpResponsePack(HttpResponse *resp, int *len, FREE **freefunc);
/* packs into a given buffer; call packedSize first to check for overflows! */
extern int httpResponsePackInto(HttpResponse *resp, char *buf);

/* do everything in one call: init, set, pack, clean */
extern char *httpPackedResponse(double ver, http_status status, const char *ctype, int clen, time_t lmt, time_t expires, int *len, FREE **freefunc);

/* Call this to know how much space to allocate. Always includes one terminating 0 */
extern int httpResponsePackedSize(HttpResponse *resp);


#if 0
/* standard replies */
HttpResponse *httpResponseCreateTrace(HttpRequest *req);
#endif

/*
 * HTTP Status-Line
 */

/* init/clean */
extern void httpStatusLineInit(HttpStatusLine *sline);
extern void httpStatusLineClean(HttpStatusLine *sline);

/* set values */
extern void httpStatusLineSet(HttpStatusLine *sline, double version, http_status status, const char *reason);

/* parse/swap */
/* parse a 0-terminating buffer and fill internal structires; returns true if successful */
extern int httpStatusLineParse(HttpStatusLine *sline, const char *start, const char *end);
/* swaps fields to store using storeAppendPrintf */
extern void httpStatusLineSwap(HttpStatusLine *sline, StoreEntry *e);

/* pack */
extern int httpStatusLinePackInto(HttpStatusLine *sline, char *buf);



/*
 * HTTP Body
 */

/* init/clean */
extern void httpBodyInit(HttpBody *body);
extern void httpBodyClean(HttpBody *body);

/* get body ptr (always use this) */
extern const char *httpBodyPtr(const HttpBody *body);

/* set body */
extern void httpBodySet(HttpBody *body, const char *content, int len);

/* swap to store */
extern void httpBodySwap(const HttpBody *body, StoreEntry *e);

/* pack */
extern int httpBodyPackInto(HttpBody *body, char *buf);



#endif /* ifndef _HTTP_REPLY_H_ */

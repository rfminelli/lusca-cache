/*
 * $Id$
 *
 * DEBUG: section ??    HTTP Response
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

HttpResponse *
httpResponseCreate()
{
    HttpResponse *resp = memAllocate(MEM_HTTP_RESPONSE, 1);
    httpResponseInit(resp);
    return resp;
}

void
httpResponseInit(HttpResponse *resp)
{
    assert(resp);
    httpStatusLineInit(&resp->sline);
    httpHeaderInit(&resp->hdr);
    httpBodyInit(&resp->body);
    resp->pstate = psReadyToParseStartLine;
}

void
httpResponseClean(HttpResponse *resp)
{
    assert(resp);
    httpStatusLineClean(&resp->sline);
    httpHeaderClean(&resp->hdr);
    httpBodyClean(&resp->body);
    resp->pstate = psError;
}

void
httpResponseDestroy(HttpResponse *resp)
{
    assert(resp);
    httpResponseClean(resp);
    memFree(MEM_HTTP_RESPONSE, resp);
}

void
httpResponseSetHeaders(HttpResponse *resp, 
    double ver, http_status status, const char *reason,
    const char *ctype, int clen, time_t lmt, time_t expires)
{
    httpStatusLineSet(&resp->sline, ver, status, reason);
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

/* summarizes response in a compact HttpReply structure */
/* Note: this should work regardless if we have body set or not */
void httpResponseSumm(HttpResponse *resp, HttpReply *summ)
{
    assert(resp && summ);
    summ->version = resp->sline.version;
    summ->code = resp->sline.status;
    summ->content_length = httpHeaderGetContentLength(&resp->hdr);
    summ->hdr_sz = httpResponsePackedSize(resp) - resp->body.packed_size; /* does not include terminator */
    summ->cache_control = resp->hdr.scc_mask;
    summ->misc_headers = resp->hdr.field_mask;
    summ->date = httpHeaderGetDate(&resp->hdr, "Date", NULL);
    summ->expires = httpHeaderGetExpires(&resp->hdr);
    summ->last_modified = httpHeaderGetDate(&resp->hdr, "Last-Modified", NULL);
    /* use nice safety features of xstrncpy */
    xstrncpy(summ->content_type, httpHeaderGetStr(&resp->hdr, "Content-type", NULL), sizeof(summ->content_type));
}

void
httpResponseSwap(HttpResponse *resp, StoreEntry *entry)
{
    assert(resp);
    httpStatusLineSwap(&resp->sline, entry);
    httpHeaderSwap(&resp->hdr, entry);
    storeAppendPrintf(entry, "\r\n");
    httpBodySwap(&resp->body, entry);
}

int
httpResponsePackedSize(HttpResponse *resp) {
    int size;
    assert(resp);
    /*
     * Note: all portions of http msg include terminating 0 in their size.
     * Terminating 0s should be skipped when packing and one terminating 0
     * should be appended after packing is done. We must adjust total size for
     * this.
     */
    size = 0;
    size += resp->sline.packed_size - 1;
    size += resp->hdr.packed_size - 1;
    size += 3 - 1; /* CRLF */
    size += resp->body.packed_size - 1;
    size++;
    return size;
}

char *
httpResponsePack(HttpResponse *resp, int *len, FREE **freefunc) {
    int size;
    char *buf;
    assert(resp);
    assert(freefunc); /* must supply storage for freefunc pointer */

    size = httpResponsePackedSize(resp);
    if (size <= 4*1024) {
	buf = memAllocate(MEM_4K_BUF, 0);
	*freefunc = memFree4K;
    } else
    if (size <= 8*1024) {
	buf = memAllocate(MEM_8K_BUF, 0);
	*freefunc = memFree8K;
    } else {
	buf = xmalloc(size);
	*freefunc = xfree;
    }
    if (len)
	*len = size - 1;
    
    httpResponsePackInto(resp, buf);
    return buf;
}

int
httpResponsePackInto(HttpResponse *resp, char *buf) {
    int expected_size;
    int size;
    assert(resp);
    assert(buf);

    expected_size = httpResponsePackedSize(resp);
    size = 0;
    size += httpStatusLinePackInto(&resp->sline, buf + size) - 1;
    size += httpHeaderPackInto(&resp->hdr, buf + size) - 1;
    xmemcpy(buf + size, "\r\n", 2); size += 3 - 1;
    size += httpBodyPackInto(&resp->body, buf + size) - 1;
    size++;

    assert(size == expected_size); /* paranoid check */
    return size;
}

/* does everything in one call: init, set, pack, clean */
char *
httpPackedResponse(double ver, http_status status, 
 const char *ctype, int clen, time_t lmt, time_t expires, int *len, FREE **freefunc)
{
    char *result = NULL;
    HttpResponse resp;
    httpResponseInit(&resp);
    httpResponseSetHeaders(&resp, ver, status, ctype, NULL, clen, lmt, expires);
    result = httpResponsePack(&resp, len, freefunc);
    httpResponseClean(&resp);
    return result;
}

/*
 * parses a 0-terminating buffer into HttpResponse. 
 * Returns:
 *      +1 -- success 
 *       0 -- need more data (partial parse)
 *      -1 -- parse error
 */
int
httpResponseParse(HttpResponse *resp, const char *parse_start, const char **parse_end_ptr)
{
    const char *blk_start, *blk_end;
    const char **parse_end_ptr_h = &blk_end;
    int parse_size = 0;
    assert(resp);
    assert(parse_start);
    assert(resp->pstate < psParsed);

    if (!parse_end_ptr) parse_end_ptr = parse_end_ptr_h;

    *parse_end_ptr = parse_start;
    if (resp->pstate == psReadyToParseStartLine) {
	if (!httpResponseIsolateStart(&parse_start, &parse_size, &blk_start, &blk_end))
	    return 0;
	if (!httpStatusLineParse(&resp->sline, blk_start, blk_end))
	    return httpResponseParseError(resp);

	*parse_end_ptr = parse_start;
        resp->pstate++;
    }
    
    if (resp->pstate == psReadyToParseHeaders) {
	if (!httpResponseIsolateHeaders(&parse_start, &parse_size, &blk_start, &blk_end))
	    return 0;
	if (!httpHeaderParse(&resp->hdr, blk_start, blk_end))
	    return httpResponseParseError(resp);

	*parse_end_ptr = parse_start;
	resp->pstate++;
    }

    /* could check here for a _small_ body that we could parse right away?? @?@ */

    return 1;
}

#if 0

/*
 * parses http "command line", returns true on success
 */
static int
httpReplyParseStart(HttpResponse *msg, const char *blk_start, const char *blk_end)
{
    HttpReply *rep = (HttpReply*) msg;
    assert(rep->rstate == rsReadyToParse); /* the only state we can be called in */
    return httpStatusLineParse(rep, blk_start, blk_end);
}


static void
httpReplySetRState(HttpResponse *msg, ReadState rstate)
{
    httpResponseSetRState(msg, rstate); /* call parent first */
    if (msg->rstate == rsParsedHeaders)
	engineProcessReply((HttpReply*)msg); /* we are ready to be processed */
}

static void
httpReplyNoteError(HttpResponse *msg, HttpReply *error)
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
    /* httpReplyNoteError((HttpResponse*)rep, error); */
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
    storeClientCopy(rep->entry, 0, 0, 4096, memAllocate(MEM_4K_BUF, 1), httpResponseNoteDataReady, rep);
    return rep;
}

static int
httpStatusLineParse(HttpReply *rep, const char *blk_start, const char *blk_end) {
    /* @?@ implement it */
    assert(0);
    return 0;
}

#endif


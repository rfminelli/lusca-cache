
/*
 * $Id$
 *
 * DEBUG: section 58    HTTP Reply (Response)
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

/* tmp hack, delete it @?@ */
#define Const

#include "squid.h"


/* local constants */

/* local routines */
static void httpReplyDoDestroy(HttpReply * rep);
static int httpReplyParseStep(HttpReply * rep, const char *parse_start, int atEnd);
static int httpReplyParseError(HttpReply * rep);
static int httpReplyIsolateStart(const char **parse_start, const char **blk_start, const char **blk_end);
static int httpReplyIsolateHeaders(const char **parse_start, const char **blk_start, const char **blk_end);


HttpReply *
httpReplyCreate()
{
    HttpReply *rep = memAllocate(MEM_HTTPREPLY);
    debug(58, 7) ("creating rep: %p\n", rep);
    httpReplyInit(rep);
    return rep;
}

void
httpReplyInit(HttpReply * rep)
{
    assert(rep);
    rep->hdr_sz = 0;
    rep->pstate = psReadyToParseStartLine;
    httpBodyInit(&rep->body);
    httpHeaderInit(&rep->hdr);
    httpStatusLineInit(&rep->sline);
}

void
httpReplyClean(HttpReply * rep)
{
    assert(rep);
    httpBodyClean(&rep->body);
    httpHeaderClean(&rep->hdr);
    httpStatusLineClean(&rep->sline);
}

void
httpReplyDestroy(HttpReply * rep)
{
    assert(rep);
    debug(58, 7) ("destroying rep: %p\n", rep);
    httpReplyClean(rep);
    httpReplyDoDestroy(rep);
}

void
httpReplyReset(HttpReply * rep)
{
    httpReplyClean(rep);
    httpReplyInit(rep);
}

/* absorb: copy the contents of a new reply to the old one, destroy new one */
void
httpReplyAbsorb(HttpReply * rep, HttpReply * new_rep)
{
    assert(rep && new_rep);
    httpReplyClean(rep);
    *rep = *new_rep;
    /* cannot use Clean() on new reply now! */
    httpReplyDoDestroy(new_rep);
}

/* parses a buffer that may not be 0-terminated */
int
httpReplyParse(HttpReply * rep, const char *buf)
{
    /*
     * this extra buffer/copy will be eliminated when headers become meta-data
     * in store. Currently we have to xstrncpy the buffer becuase store.c may
     * feed a non 0-terminated buffer to us @?@.
     */
    char *headers = memAllocate(MEM_4K_BUF);
    int success;
    /* reset current state, because we are not used in incremental fashion */
    httpReplyReset(rep);
    /* put a 0-terminator */
    xstrncpy(headers, buf, 4096);
    success = httpReplyParseStep(rep, headers, 0);
    memFree(MEM_4K_BUF, headers);
    return success == 1;
}

void
httpReplyPackInto(const HttpReply * rep, Packer * p)
{
    assert(rep);
    httpStatusLinePackInto(&rep->sline, p);
    httpHeaderPackInto(&rep->hdr, p);
    packerAppend(p, "\r\n", 2);
    httpBodyPackInto(&rep->body, p);
}

/* create memBuf, create mem-based packer,  pack, destroy packer, return MemBuf */
MemBuf
httpReplyPack(const HttpReply * rep)
{
    MemBuf mb;
    Packer p;
    assert(rep);

    memBufDefInit(&mb);
    packerToMemInit(&p, &mb);
    httpReplyPackInto(rep, &p);
    packerClean(&p);
    return mb;
}

/* swap: create swap-based packer, pack, destroy packer */
void
httpReplySwapOut(const HttpReply * rep, StoreEntry * e)
{
    Packer p;
    assert(rep && e);

    packerToStoreInit(&p, e);
    httpReplyPackInto(rep, &p);
    packerClean(&p);
}

MemBuf
httpPackedReply(double ver, http_status status, const char *ctype,
    int clen, time_t lmt, time_t expires)
{
    HttpReply *rep = httpReplyCreate();
    MemBuf mb;
    httpReplySetHeaders(rep, ver, status, ctype, NULL, clen, lmt, expires);
    mb = httpReplyPack(rep);
    httpReplyDestroy(rep);
    return mb;
}

MemBuf
httpPacked304Reply(const HttpReply * rep)
{
    static const http_hdr_type ImsEntries[] = { HDR_DATE, HDR_CONTENT_LENGTH, HDR_CONTENT_TYPE, HDR_EXPIRES, HDR_LAST_MODIFIED, /* eof */ HDR_OTHER };
    http_hdr_type t;
    MemBuf mb;
    Packer p;
    HttpHeaderEntry *e;
    assert(rep);

    memBufDefInit(&mb);
    packerToMemInit(&p, &mb);
    memBufPrintf(&mb, "%s", "HTTP/1.0 304 Not Modified\r\n");
    for (t = 0; ImsEntries[t] != HDR_OTHER; ++t)
	if ((e = httpHeaderFindEntry(&rep->hdr, ImsEntries[t], NULL)))
	    httpHeaderEntryPackInto(e, &p);
    memBufAppend(&mb, "\r\n", 2);
    packerClean(&p);
    return mb;
}

void
httpReplySetHeaders(HttpReply * reply, double ver, http_status status, const char *reason,
    const char *ctype, int clen, time_t lmt, time_t expires)
{
    HttpHeader *hdr;
    assert(reply);
    httpStatusLineSet(&reply->sline, ver, status, reason);
    hdr = &reply->hdr;
    httpHeaderAddExt(hdr, "Server", full_appname_string);
    httpHeaderAddExt(hdr, "MIME-Version", "1.0");	/* do we need this? @?@ */
    httpHeaderSetTime(hdr, HDR_DATE, squid_curtime);
    if (ctype)
	httpHeaderSetStr(hdr, HDR_CONTENT_TYPE, ctype);
    if (clen >= 0)
	httpHeaderSetInt(hdr, HDR_CONTENT_LENGTH, clen);
    if (expires >= 0)
	httpHeaderSetTime(hdr, HDR_EXPIRES, expires);
    if (lmt > 0)		/* this used to be lmt != 0 @?@ */
	httpHeaderSetTime(hdr, HDR_LAST_MODIFIED, lmt);
}

/*
 * header manipulation 
 *
 * never go to header directly if you can use these:
 *
 * our interpretation of headers often changes and you may get into trouble
 *    if you, for example, assume that HDR_EXPIRES contains expire info
 *
 * if you think about it, in most cases, you are not looking for the information
 *    in the header, but rather for current state of the reply, which may or may 
 *    not depend on headers. 
 *
 * For example, the _real_ question is
 *        "when does this object expire?" 
 *     not 
 *        "what is the value of the 'Expires:' header?"
 */

void
httpReplyUpdateOnNotModified(HttpReply * rep, HttpReply * freshRep)
{
#if 0 /* this is what we want: */
    rep->cache_control = freshRep->cache_control;
    rep->misc_headers = freshRep->misc_headers;
    if (freshRep->date > -1)
	rep->date = freshRep->date;
    if (freshRep->last_modified > -1)
	rep->last_modified = freshRep->last_modified;
    if (freshRep->expires > -1)
	rep->expires = freshRep->expires;
#endif
    time_t date;
    time_t expires;
    time_t lmt;
    assert(rep && freshRep);
    /* save precious info */
    date = httpHeaderGetTime(&rep->hdr, HDR_DATE);
    expires = httpReplyExpires(rep);
    lmt = httpHeaderGetTime(&rep->hdr, HDR_LAST_MODIFIED);
    /* clean old headers */
    httpHeaderClean(&rep->hdr);
    httpHeaderInit(&rep->hdr);
    /* copy */
    httpHeaderCopy(&rep->hdr, &freshRep->hdr);
    /* restore missing info if needed */
    if (!httpHeaderHas(&rep->hdr, HDR_DATE))
	httpHeaderSetTime(&rep->hdr, HDR_DATE, date);
    if (!httpHeaderHas(&rep->hdr, HDR_EXPIRES))
	httpHeaderSetTime(&rep->hdr, HDR_EXPIRES, expires);
    if (!httpHeaderHas(&rep->hdr, HDR_LAST_MODIFIED))
	httpHeaderSetTime(&rep->hdr, HDR_LAST_MODIFIED, lmt);
}

int
httpReplyContentLen(const HttpReply * rep)
{
    assert(rep);
    return httpHeaderGetInt(&rep->hdr, HDR_CONTENT_LENGTH);
}

/* should we return "" or NULL if no content-type? Return NULL for now @?@ */
const char *
httpReplyContentType(const HttpReply * rep)
{
    assert(rep);
    return httpHeaderGetStr(&rep->hdr, HDR_CONTENT_TYPE);
}

time_t
httpReplyExpires(const HttpReply * rep)
{
    HttpHdrCc *cc;
    time_t exp = -1;
    assert(rep);
    /* The max-age directive takes priority over Expires, check it first */
    cc = httpHeaderGetCc(&rep->hdr);
    if (cc)
	exp = cc->max_age;
    if (exp < 0)
	exp = httpHeaderGetTime(&rep->hdr, HDR_EXPIRES);
    return exp;
}

int
httpReplyHasCc(const HttpReply * rep, http_hdr_cc_type type)
{
    HttpHdrCc *cc;
    assert(rep);
    assert(type >= 0 && type < CC_ENUM_END);

    cc = httpHeaderGetCc(&rep->hdr);
    return cc &&		/* scc header is present */
	EBIT_TEST(cc->mask, type);
}


/* internal routines */

/* internal function used by Destroy and Absorb */
static void
httpReplyDoDestroy(HttpReply * rep)
{
    memFree(MEM_HTTPREPLY, rep);
}

/*
 * parses a 0-terminating buffer into HttpReply. 
 * Returns:
 *      +1 -- success 
 *       0 -- need more data (partial parse)
 *      -1 -- parse error
 */
static int
httpReplyParseStep(HttpReply * rep, const char *buf, int atEnd)
{
    const char *parse_start = buf;
    const char *blk_start, *blk_end;
    const char **parse_end_ptr = &blk_end;
    assert(rep);
    assert(parse_start);
    assert(rep->pstate < psParsed);

    *parse_end_ptr = parse_start;
    if (rep->pstate == psReadyToParseStartLine) {
	if (!httpReplyIsolateStart(&parse_start, &blk_start, &blk_end))
	    return 0;
	if (!httpStatusLineParse(&rep->sline, blk_start, blk_end))
	    return httpReplyParseError(rep);

	*parse_end_ptr = parse_start;
	rep->hdr_sz = *parse_end_ptr - buf;
	rep->pstate++;
    }
    if (rep->pstate == psReadyToParseHeaders) {
	if (!httpReplyIsolateHeaders(&parse_start, &blk_start, &blk_end))
	    if (atEnd)
		blk_start = parse_start, blk_end = blk_start + strlen(blk_start);
	    else
		return 0;
	if (!httpHeaderParse(&rep->hdr, blk_start, blk_end))
	    return httpReplyParseError(rep);

	*parse_end_ptr = parse_start;
	rep->hdr_sz = *parse_end_ptr - buf;
	rep->pstate++;
    }
    return 1;
}


/* handy: resets and returns -1 */
static int
httpReplyParseError(HttpReply * rep)
{
    assert(rep);
    /* reset */
    httpReplyReset(rep);
    /* indicate an error */
    rep->sline.status = HTTP_INVALID_HEADER;
    return -1;
}

/* find first CRLF */
static int
httpReplyIsolateStart(const char **parse_start, const char **blk_start, const char **blk_end)
{
    int slen = strcspn(*parse_start, "\r\n");
    if (!(*parse_start)[slen])	/* no CRLF found */
	return 0;

    *blk_start = *parse_start;
    *blk_end = *blk_start + slen;
    if (**blk_end == '\r')	/* CR */
	(*blk_end)++;
    if (**blk_end == '\n')	/* LF */
	(*blk_end)++;

    *parse_start = *blk_end;
    return 1;
}

/* find end of headers */
static int
httpReplyIsolateHeaders(const char **parse_start, const char **blk_start, const char **blk_end)
{
    /* adopted with mods from mime_headers_end() */
    const char *p1 = strstr(*parse_start, "\n\r\n");
    const char *p2 = strstr(*parse_start, "\n\n");
    const char *end = NULL;

    if (p1 && p2)
	end = p1 < p2 ? p1 : p2;
    else
	end = p1 ? p1 : p2;

    if (end) {
	*blk_start = *parse_start;
	*blk_end = end + 1;
	*parse_start = end + (end == p1 ? 3 : 2);
    }
    return end != NULL;
}

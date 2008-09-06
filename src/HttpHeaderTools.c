
/*
 * $Id$
 *
 * DEBUG: section 66    HTTP Header Tools
 * AUTHOR: Alex Rousskov
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

#include "squid.h"

#if UNUSED_CODE
static int httpHeaderStrCmp(const char *h1, const char *h2, int len);
#endif
static void httpHeaderPutStrvf(HttpHeader * hdr, http_hdr_type id, const char *fmt, va_list vargs);

/* same as httpHeaderPutStr, but formats the string using snprintf first */
void
#if STDC_HEADERS
httpHeaderPutStrf(HttpHeader * hdr, http_hdr_type id, const char *fmt,...)
#else
httpHeaderPutStrf(va_alist)
     va_dcl
#endif
{
#if STDC_HEADERS
    va_list args;
    va_start(args, fmt);
#else
    va_list args;
    HttpHeader *hdr = NULL;
    http_hdr_type id = HDR_ENUM_END;
    const char *fmt = NULL;
    va_start(args);
    hdr = va_arg(args, HttpHeader *);
    id = va_arg(args, http_hdr_type);
    fmt = va_arg(args, char *);
#endif
    httpHeaderPutStrvf(hdr, id, fmt, args);
    va_end(args);
}

/* used by httpHeaderPutStrf */
static void
httpHeaderPutStrvf(HttpHeader * hdr, http_hdr_type id, const char *fmt, va_list vargs)
{
    MemBuf mb;
    memBufDefInit(&mb);
    memBufVPrintf(&mb, fmt, vargs);
    httpHeaderPutStr(hdr, id, mb.buf);
    memBufClean(&mb);
}


/* wrapper arrounf PutContRange */
void
httpHeaderAddContRange(HttpHeader * hdr, HttpHdrRangeSpec spec, squid_off_t ent_len)
{
    HttpHdrContRange *cr = httpHdrContRangeCreate();
    assert(hdr && ent_len >= 0);
    httpHdrContRangeSet(cr, spec, ent_len);
    httpHeaderPutContRange(hdr, cr);
    httpHdrContRangeDestroy(cr);
}


/*
 * return true if a given directive is found in at least one of
 * the "connection" header-fields note: if HDR_PROXY_CONNECTION is
 * present we ignore HDR_CONNECTION.
 */
int
httpHeaderHasConnDir(const HttpHeader * hdr, const char *directive)
{
    String list;
    http_hdr_type ht;
    int res;
    /* what type of header do we have? */
    if (httpHeaderHas(hdr, HDR_PROXY_CONNECTION))
	ht = HDR_PROXY_CONNECTION;
    else if (httpHeaderHas(hdr, HDR_CONNECTION))
	ht = HDR_CONNECTION;
    else
	return 0;

    list = httpHeaderGetList(hdr, ht);
    res = strListIsMember(&list, directive, ',');
    stringClean(&list);
    return res;
}

/* handy to printf prefixes of potentially very long buffers */
const char *
getStringPrefix(const char *str, const char *end)
{
#define SHORT_PREFIX_SIZE 512
    LOCAL_ARRAY(char, buf, SHORT_PREFIX_SIZE);
    const int sz = 1 + (end ? end - str : strlen(str));
    xstrncpy(buf, str, (sz > SHORT_PREFIX_SIZE) ? SHORT_PREFIX_SIZE : sz);
    return buf;
}

/*
 * parses a given string then packs compiled headers and compares the result
 * with the original, reports discrepancies
 */
#if UNUSED_CODE
void
httpHeaderTestParser(const char *hstr)
{
    static int bug_count = 0;
    int hstr_len;
    int parse_success;
    HttpHeader hdr;
    int pos;
    Packer p;
    MemBuf mb;
    assert(hstr);
    /* skip start line if any */
    if (!strncasecmp(hstr, "HTTP/", 5)) {
	const char *p = strchr(hstr, '\n');
	if (p)
	    hstr = p + 1;
    }
    /* skip invalid first line if any */
    if (xisspace(*hstr)) {
	const char *p = strchr(hstr, '\n');
	if (p)
	    hstr = p + 1;
    }
    hstr_len = strlen(hstr);
    /* skip terminator if any */
    if (strstr(hstr, "\n\r\n"))
	hstr_len -= 2;
    else if (strstr(hstr, "\n\n"))
	hstr_len -= 1;
    httpHeaderInit(&hdr, hoReply);
    /* debugLevels[55] = 8; */
    parse_success = httpHeaderParse(&hdr, hstr, hstr + hstr_len);
    /* debugLevels[55] = 2; */
    if (!parse_success) {
	debug(66, 2) ("TEST (%d): failed to parsed a header: {\n%s}\n", bug_count, hstr);
	return;
    }
    /* we think that we parsed it, veryfy */
    memBufDefInit(&mb);
    packerToMemInit(&p, &mb);
    httpHeaderPackInto(&hdr, &p);
    if ((pos = abs(httpHeaderStrCmp(hstr, mb.buf, hstr_len)))) {
	bug_count++;
	debug(66, 2) ("TEST (%d): hdr parsing bug (pos: %d near '%s'): expected: {\n%s} got: {\n%s}\n",
	    bug_count, pos, hstr + pos, hstr, mb.buf);
    }
    httpHeaderClean(&hdr);
    packerClean(&p);
    memBufClean(&mb);
}
#endif


/* like strncasecmp but ignores ws characters */
#if UNUSED_CODE
static int
httpHeaderStrCmp(const char *h1, const char *h2, int len)
{
    int len1 = 0;
    int len2 = 0;
    assert(h1 && h2);
    /* fast check first */
    if (!strncasecmp(h1, h2, len))
	return 0;
    while (1) {
	const char c1 = xtoupper(h1[len1 += xcountws(h1 + len1)]);
	const char c2 = xtoupper(h2[len2 += xcountws(h2 + len2)]);
	if (c1 < c2)
	    return -len1;
	if (c1 > c2)
	    return +len1;
	if (!c1 && !c2)
	    return 0;
	if (c1)
	    len1++;
	if (c2)
	    len2++;
    }
    /* NOTREACHED */
    return 0;
}
#endif

/*
 * httpHdrMangle checks the anonymizer (header_access) configuration.
 * Returns 1 if the header is allowed.
 */
static int
httpHdrMangle(HttpHeaderEntry * e, request_t * request)
{
    int retval = 1;

    /* check with anonymizer tables */
    header_mangler *hm;
    assert(e);
    if (e->id == HDR_OTHER) {
	for (hm = Config.header_access[HDR_OTHER].next; hm; hm = hm->next) {
	    if (strCmp(e->name, hm->name) == 0)
		break;
	}
	if (!hm)
	    return 1;
    } else
	hm = &Config.header_access[e->id];
    if (!hm->access_list)
	return 1;
    if (aclCheckFastRequest(hm->access_list, request)) {
	retval = 1;
    } else if (NULL == hm->replacement) {
	/* It was denied, and we don't have any replacement */
	retval = 0;
    } else {
	/* It was denied, but we have a replacement. Replace the
	 * header on the fly, and return that the new header
	 * is allowed.
	 */
	stringReset(&e->value, hm->replacement);
	retval = -1;
    }

    return retval != 0;
}

/* Mangles headers for a list of headers. */
void
httpHdrMangleList(HttpHeader * l, request_t * request)
{
    HttpHeaderEntry *e;
    HttpHeaderPos p = HttpHeaderInitPos;
    int removed_headers = 0;
    while ((e = httpHeaderGetEntry(l, &p))) {
	if (0 == httpHdrMangle(e, request)) {
	    httpHeaderDelAt(l, p);
	    removed_headers++;
	}
    }
    if (removed_headers)
	httpHeaderRefreshMask(l);
}

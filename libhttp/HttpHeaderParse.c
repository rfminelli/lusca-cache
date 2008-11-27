
/*
 * $Id: HttpHeader.c 12651 2008-04-25 16:47:11Z adrian.chadd $
 *
 * DEBUG: section 55    HTTP Header
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
#include "../include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../include/Array.h"
#include "../include/Stack.h"
#include "../include/util.h"
#include "../libcore/valgrind.h"
#include "../libcore/varargs.h"
#include "../libcore/debug.h"
#include "../libcore/kb.h"
#include "../libcore/gb.h"
#include "../libcore/tools.h"

#include "../libmem/MemPool.h"
#include "../libmem/MemBufs.h"
#include "../libmem/MemBuf.h"
#include "../libmem/String.h"

#include "../libcb/cbdata.h"

#include "../libstat/StatHist.h"

#include "HttpVersion.h"
#include "HttpStatusLine.h"
#include "HttpHeaderType.h"
#include "HttpHeaderFieldStat.h"
#include "HttpHeaderFieldInfo.h"
#include "HttpHeaderEntry.h"
#include "HttpHeader.h"
#include "HttpHeaderStats.h"
#include "HttpHeaderTools.h"
#include "HttpHeaderParse.h"

int httpConfig_relaxed_parser = 0;
int HeaderEntryParsedCount = 0;

int
httpHeaderParse(HttpHeader * hdr, const char *header_start, const char *header_end)
{
    const char *field_ptr = header_start;
    HttpHeaderEntry *e;

    assert(hdr);
    assert(header_start && header_end);
    debug(55, 7) ("parsing hdr: (%p)\n%s\n", hdr, getStringPrefix(header_start, header_end));
    HttpHeaderStats[hdr->owner].parsedCount++;
    if (memchr(header_start, '\0', header_end - header_start)) {
	debug(55, 1) ("WARNING: HTTP header contains NULL characters {%s}\n",
	    getStringPrefix(header_start, header_end));
	return httpHeaderReset(hdr);
    }
    /* common format headers are "<name>:[ws]<value>" lines delimited by <CRLF>.
     * continuation lines start with a (single) space or tab */
    while (field_ptr < header_end) {
	const char *field_start = field_ptr;
	const char *field_end;
	do {
	    const char *this_line = field_ptr;
	    field_ptr = memchr(field_ptr, '\n', header_end - field_ptr);
	    if (!field_ptr)
		return httpHeaderReset(hdr);	/* missing <LF> */
	    field_end = field_ptr;
	    field_ptr++;	/* Move to next line */
	    if (field_end > this_line && field_end[-1] == '\r') {
		field_end--;	/* Ignore CR LF */
		/* Ignore CR CR LF in relaxed mode */
		if (httpConfig_relaxed_parser && field_end > this_line + 1 && field_end[-1] == '\r') {
		    debug(55, httpConfig_relaxed_parser <= 0 ? 1 : 2)
			("WARNING: Double CR characters in HTTP header {%s}\n", getStringPrefix(field_start, field_end));
		    field_end--;
		}
	    }
	    /* Barf on stray CR characters */
	    if (memchr(this_line, '\r', field_end - this_line)) {
		debug(55, 1) ("WARNING: suspicious CR characters in HTTP header {%s}\n",
		    getStringPrefix(field_start, field_end));
		if (httpConfig_relaxed_parser) {
		    char *p = (char *) this_line;	/* XXX Warning! This destroys original header content and violates specifications somewhat */
		    while ((p = memchr(p, '\r', field_end - p)) != NULL)
			*p++ = ' ';
		} else
		    return httpHeaderReset(hdr);
	    }
	    if (this_line + 1 == field_end && this_line > field_start) {
		debug(55, 1) ("WARNING: Blank continuation line in HTTP header {%s}\n",
		    getStringPrefix(header_start, header_end));
		return httpHeaderReset(hdr);
	    }
	} while (field_ptr < header_end && (*field_ptr == ' ' || *field_ptr == '\t'));
	if (field_start == field_end) {
	    if (field_ptr < header_end) {
		debug(55, 1) ("WARNING: unparseable HTTP header field near {%s}\n",
		    getStringPrefix(field_start, header_end));
		return httpHeaderReset(hdr);
	    }
	    break;		/* terminating blank line */
	}
	e = httpHeaderEntryParseCreate(field_start, field_end);
	if (NULL == e) {
	    debug(55, 1) ("WARNING: unparseable HTTP header field {%s}\n",
		getStringPrefix(field_start, field_end));
	    debug(55, httpConfig_relaxed_parser <= 0 ? 1 : 2)
		(" in {%s}\n", getStringPrefix(header_start, header_end));
	    if (httpConfig_relaxed_parser)
		continue;
	    else
		return httpHeaderReset(hdr);
	}
	if (e->id == HDR_CONTENT_LENGTH) {
	    squid_off_t l1;
	    HttpHeaderEntry *e2;
	    if (!httpHeaderParseSize(strBuf(e->value), &l1)) {
		debug(55, 1) ("WARNING: Unparseable content-length '%s'\n", strBuf(e->value));
		httpHeaderEntryDestroy(e);
		return httpHeaderReset(hdr);
	    }
	    e2 = httpHeaderFindEntry(hdr, e->id);
	    if (e2 && strCmp(e->value, strBuf(e2->value)) != 0) {
		squid_off_t l2;
		debug(55, httpConfig_relaxed_parser <= 0 ? 1 : 2) ("WARNING: found two conflicting content-length headers in {%s}\n", getStringPrefix(header_start, header_end));
		if (!httpConfig_relaxed_parser) {
		    httpHeaderEntryDestroy(e);
		    return httpHeaderReset(hdr);
		}
		if (!httpHeaderParseSize(strBuf(e2->value), &l2)) {
		    debug(55, 1) ("WARNING: Unparseable content-length '%s'\n", strBuf(e->value));
		    httpHeaderEntryDestroy(e);
		    return httpHeaderReset(hdr);
		}
		if (l1 > l2) {
		    httpHeaderDelById(hdr, e2->id);
		} else {
		    httpHeaderEntryDestroy(e);
		    continue;
		}
	    } else if (e2) {
		debug(55, httpConfig_relaxed_parser <= 0 ? 1 : 2)
		    ("NOTICE: found double content-length header\n");
		if (httpConfig_relaxed_parser) {
		    httpHeaderEntryDestroy(e);
		    continue;
		} else {
		    httpHeaderEntryDestroy(e);
		    return httpHeaderReset(hdr);
		}
	    }
	}
	if (e->id == HDR_OTHER && stringHasWhitespace(strBuf(e->name))) {
	    debug(55, httpConfig_relaxed_parser <= 0 ? 1 : 2)
		("WARNING: found whitespace in HTTP header name {%s}\n", getStringPrefix(field_start, field_end));
	    if (!httpConfig_relaxed_parser) {
		httpHeaderEntryDestroy(e);
		return httpHeaderReset(hdr);
	    }
	}
	httpHeaderAddEntry(hdr, e);
    }
    return 1;			/* even if no fields where found, it is a valid header */
}

/*
 * HttpHeaderEntry
 */

/* parses and inits header entry, returns new entry on success */
HttpHeaderEntry *
httpHeaderEntryParseCreate(const char *field_start, const char *field_end)
{
    HttpHeaderEntry *e;
    int id;
    /* note: name_start == field_start */
    const char *name_end = memchr(field_start, ':', field_end - field_start);
    int name_len = name_end ? name_end - field_start : 0;
    const char *value_start = field_start + name_len + 1;	/* skip ':' */
    /* note: value_end == field_end */

    HeaderEntryParsedCount++;

    /* do we have a valid field name within this field? */
    if (!name_len || name_end > field_end)
	return NULL;
    if (name_len > 65534) {
	/* String must be LESS THAN 64K and it adds a terminating NULL */
	debug(55, 1) ("WARNING: ignoring header name of %d bytes\n", name_len);
	return NULL;
    }
    if (httpConfig_relaxed_parser && xisspace(field_start[name_len - 1])) {
	debug(55, httpConfig_relaxed_parser <= 0 ? 1 : 2)
	    ("NOTICE: Whitespace after header name in '%s'\n", getStringPrefix(field_start, field_end));
	while (name_len > 0 && xisspace(field_start[name_len - 1]))
	    name_len--;
	if (!name_len)
	    return NULL;
    }
    /* now we know we can parse it */
    e = memPoolAlloc(pool_http_header_entry);
    debug(55, 9) ("creating entry %p: near '%s'\n", e, getStringPrefix(field_start, field_end));
    /* is it a "known" field? */
    id = httpHeaderIdByName(field_start, name_len, Headers, HDR_ENUM_END);
    if (id < 0)
	id = HDR_OTHER;
    assert_eid(id);
    e->id = id;
    /* set field name */
    if (id == HDR_OTHER)
	stringLimitInit(&e->name, field_start, name_len);
    else
	e->name = Headers[id].name;
    /* trim field value */
    while (value_start < field_end && xisspace(*value_start))
	value_start++;
    while (value_start < field_end && xisspace(field_end[-1]))
	field_end--;
    if (field_end - value_start > 65534) {
	/* String must be LESS THAN 64K and it adds a terminating NULL */
	debug(55, 1) ("WARNING: ignoring '%s' header of %d bytes\n",
	    strBuf(e->name), (int) (field_end - value_start));
	if (e->id == HDR_OTHER)
	    stringClean(&e->name);
	memPoolFree(pool_http_header_entry, e);
	return NULL;
    }
    /* set field value */
    stringLimitInit(&e->value, value_start, field_end - value_start);
    Headers[id].stat.seenCount++;
    Headers[id].stat.aliveCount++;
    debug(55, 9) ("created entry %p: '%s: %s'\n", e, strBuf(e->name), strBuf(e->value));
    return e;
}

/*
 * parses an int field, complains if soemthing went wrong, returns true on
 * success
 */
int
httpHeaderParseInt(const char *start, int *value)
{
    char *end;
    long v;
    assert(value);
    errno = 0;
    v = *value = strtol(start, &end, 10);
    if (start == end || errno != 0 || v != *value) {
        debug(66, 2) ("failed to parse an int header field near '%s'\n", start);
        *value = -1;
        return 0;
    }
    return 1;
}

int
httpHeaderParseSize(const char *start, squid_off_t * value)
{
    char *end;
    errno = 0;
    assert(value);
    *value = strto_off_t(start, &end, 10);
    if (start == end || errno != 0) {
        debug(66, 2) ("failed to parse an int header field near '%s'\n", start);
        *value = -1;
        return 0;
    }
    return 1;
}


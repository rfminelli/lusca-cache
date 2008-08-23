
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

#include "HttpVersion.h"
#include "HttpStatusLine.h"
#include "HttpHeaderType.h"
#include "HttpHeaderFieldStat.h"
#include "HttpHeaderFieldInfo.h"
#include "HttpHeaderEntry.h"
#include "HttpHeader.h"
#include "HttpHeaderTools.h"

HttpHeaderFieldInfo *Headers = NULL;
MemPool * pool_http_header_entry = NULL;

#define assert_eid(id) assert((id) < HDR_ENUM_END)

void
httpHeaderInitLibrary(void)
{
#if NOTYET
    /* all headers must be described */
    assert(countof(HeadersAttrs) == HDR_ENUM_END);
#endif
    if (!Headers)
        Headers = httpHeaderBuildFieldsInfo(HeadersAttrs, HDR_ENUM_END);
    if (! pool_http_header_entry)
        pool_http_header_entry = memPoolCreate("HttpHeaderEntry", sizeof(HttpHeaderEntry));
}

/* appends an entry;
 * does not call httpHeaderEntryClone() so one should not reuse "*e"
 */
void
httpHeaderAddEntry(HttpHeader * hdr, HttpHeaderEntry * e)
{
    assert(hdr && e);
    assert_eid(e->id);

    debug(55, 7) ("%p adding entry: %d at %d\n",
        hdr, e->id, hdr->entries.count);
    if (CBIT_TEST(hdr->mask, e->id))
        Headers[e->id].stat.repCount++;
    else
        CBIT_SET(hdr->mask, e->id);
    arrayAppend(&hdr->entries, e);
    /* increment header length, allow for ": " and crlf */
    hdr->len += strLen(e->name) + 2 + strLen(e->value) + 2;
}

/*!
 * @function
 *	httpHeaderAddEntryStr
 * @abstract
 *	Append a header entry to the give hdr.
 *
 * @param	hdr		HttpHeader to append the entry to.
 * @param	id		http_hdr_type; HDR_OTHER == uses NUL-terminated attrib;
 *				else attrib is ignored.
 * @param	value		NUL-terminated header value.
 *
 * @discussion
 *	The attrib/value strings will be copied even if the values passed in are static.
 *
 *	strlen() will be run on the strings as appropriate. Call httpHeaderEntryAddStr2() if
 *	the length of the string is known up front.
 *
 *	For now this routine simply calls httpHeaderEntryCreate() to create the entry and
 *	then httpHeaderAddEntry() to add it; the plan is to eliminate the httpHeaderEntryCreate()
 *	allocator overhead.
 */
void
httpHeaderAddEntryStr(HttpHeader *hdr, http_hdr_type id, const char *attrib, const char *value)
{
	httpHeaderAddEntry(hdr, httpHeaderEntryCreate(id, attrib, value));
}

/*!
 * @function
 *	httpHeaderInsertEntryStr
 * @abstract
 *	Insert a header entry into the give hdr at position pos
 *
 * @param	hdr		HttpHeader to append the entry to.
 * @param	pos		position in the array to insert.
 * @param	id		http_hdr_type; HDR_OTHER == uses NUL-terminated attrib;
 *				else attrib is ignored.
 * @param	value		NUL-terminated header value.
 *
 * @discussion
 *	The attrib/value strings will be copied even if the values passed in are static.
 *
 *	strlen() will be run on the strings as appropriate. Call httpHeaderEntryAddStr2() if
 *	the length of the string is known up front.
 *
 *	For now this routine simply calls httpHeaderEntryCreate() to create the entry and
 *	then httpHeaderInsertEntry() to add it; the plan is to eliminate the httpHeaderEntryCreate()
 *	allocator overhead.
 */
void
httpHeaderInsertEntryStr(HttpHeader *hdr, int pos, http_hdr_type id, const char *attrib, const char *value)
{
	httpHeaderInsertEntry(hdr, httpHeaderEntryCreate(id, attrib, value), pos);
}

/* inserts an entry at the given position;
 * does not call httpHeaderEntryClone() so one should not reuse "*e"
 */
void
httpHeaderInsertEntry(HttpHeader * hdr, HttpHeaderEntry * e, int pos)
{
    assert(hdr && e);
    assert_eid(e->id);

    debug(55, 7) ("%p adding entry: %d at %d\n",
        hdr, e->id, hdr->entries.count);
    if (CBIT_TEST(hdr->mask, e->id))
        Headers[e->id].stat.repCount++;
    else
        CBIT_SET(hdr->mask, e->id);
    arrayInsert(&hdr->entries, e, pos);
    /* increment header length, allow for ": " and crlf */
    hdr->len += strLen(e->name) + 2 + strLen(e->value) + 2;
}

/* returns next valid entry */
HttpHeaderEntry *
httpHeaderGetEntry(const HttpHeader * hdr, HttpHeaderPos * pos)
{
    assert(hdr && pos);
    assert(*pos >= HttpHeaderInitPos && *pos < hdr->entries.count);
    for ((*pos)++; *pos < hdr->entries.count; (*pos)++) {
        if (hdr->entries.items[*pos])
            return hdr->entries.items[*pos];
    }
    return NULL;
}

void
httpHeaderAddClone(HttpHeader * hdr, const HttpHeaderEntry * e)
{
    httpHeaderAddEntry(hdr, httpHeaderEntryClone(e));
}


/*
 * $Id$
 *
 * DEBUG: section ??    HTTP Header
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

/* To-do:
 *   - recycle headers, not just strings in fields?
 */

#include "squid.h"
#include "MemPool.h"
#include "HttpHeader.h"

static const char *const crlf = "\r\n";

/* headers accounting */
#define INIT_FIELDS_PER_HEADER 32
static u_num32 shortHeadersCount = 0;
static u_num32 longHeadersCount = 0;

/* recycle bean for short strings */
static const size_t shortStrSize = 24;
static const size_t shortStrPoolCount = 1024*1024L/24;
static MemPool *shortStrings = NULL;

/* long strings accounting */
static u_num32 longStrAllocCount = 0;
static u_num32 longStrFreeCount = 0;
static u_num32 longStrHighWaterCount = 0;
static size_t longStrAllocSize = 0;
static size_t longStrFreeSize = 0;
static size_t longStrHighWaterSize = 0;


/* local routines */
static void httpHeaderGrow(HttpHeader *hdr);
static HttpHeaderField *httpHeaderFieldCreate(const char *name, const char *value);
static void httpHeaderFieldDestroy(HttpHeaderField *f);
static char *dupShortString(const char *str);
static void freeShortString(char *str);

/* delete this when everybody remembers that ':' is not a part of a name */
#define converion_period_name_check(name) assert(!strchr((name), ':'))

HttpHeader *
httpHeaderCreate()
{
    HttpHeader *hdr = xcalloc(1, sizeof(HttpHeader));
    /* all members are set to 0 in calloc */

    /* check if pool is ready (no static init in C??) */
    if (!shortStrings)
	shortStrings = memPoolCreate(shortStrPoolCount, shortStrPoolCount/10, shortStrSize, "shortStr");

    return hdr;
}

int 
httpHeaderParse(HttpHeader *hdr, const char *buf, size_t size)
{
    assert(hdr);
    assert(buf);

    assert(0); /* not implemented yet */

    return 0;
}

void 
httpHeaderDestroy(HttpHeader *hdr)
{
    HttpHeaderPos pos = httpHeaderInitPos;
    HttpHeaderField *f;

    assert(hdr);

    if (hdr->capacity > INIT_FIELDS_PER_HEADER)
	longHeadersCount++;
    else
	shortHeadersCount++;

    while ((f = httpHeaderGetField(hdr, 0, 0, &pos)))
	httpHeaderFieldDestroy(f);

    xfree(hdr);
    /* maybe we should recycle headers too ? */
}

const char *
httpHeaderGetStr(HttpHeader *hdr, const char *name, HttpHeaderPos *pos)
{
    const char *n;
    const char *v;

    assert(hdr);
    assert(name);
    converion_period_name_check(name);

    while(httpHeaderGetField(hdr, &n, &v, pos))
    	if (strcasecmp(name, n) == 0)
    	    return v;
    return NULL;
}

long 
httpHeaderGetInt(HttpHeader *hdr, const char *name, HttpHeaderPos *pos)
{
    const char *str = httpHeaderGetStr(hdr, name, pos);
    return str ? atol(str) : -1;
}


HttpHeaderField *
httpHeaderGetField(HttpHeader *hdr, const char **name, const char **value, HttpHeaderPos *pos)
{
    /* we do not want to care about '!pos' in the loop: */
    HttpHeaderPos p = httpHeaderInitPos;
    if (!pos) pos = &p;
    
    assert(hdr);

    (*pos)++;
    assert(*pos>=0);
    while (*pos < hdr->count)
    	if (hdr->fields[*pos]) {
    	    if (name) *name = hdr->fields[*pos]->name;
    	    if (value) *value = hdr->fields[*pos]->value;
    	    return hdr->fields[*pos];
    	}

    if (name) name = NULL;
    if (value) value = NULL;
    return NULL;
}

/* deletes all field(s) with name if any, returns #fields deleted */
int 
httpHeaderDelFields(HttpHeader *hdr, const char *name)
{
    int count = 0;
    HttpHeaderPos pos = httpHeaderInitPos;

    while (httpHeaderGetStr(hdr, name, &pos)) {
    	httpHeaderDelField(hdr, pos);
    	count++;
    }
    return count;
}

/*
 * deletes field by pos and leaves a gap (NULL field); leaving a gap makes it
 * possible to iterate(search) and delete fields at the same time
 */
void
httpHeaderDelField(HttpHeader *hdr, HttpHeaderPos pos)
{
    assert(hdr);
    assert(pos >= 0 && pos < hdr->count);

    httpHeaderFieldDestroy(hdr->fields[pos]);
    hdr->fields[pos] = NULL;
}

/* add a field (appends) */
const char *
httpHeaderAddStrField(HttpHeader *hdr, const char *name, const char *value)
{

    assert(hdr);
    assert(name);
    assert(value);

    if (hdr->count >= hdr->capacity)
	httpHeaderGrow(hdr);

    hdr->fields[hdr->count++] = httpHeaderFieldCreate(name, value);
    return value;
}

long
httpHeaderAddIntField(HttpHeader *hdr, const char *name, long value) {
    static char buf[32]; /* 2^64 = 18446744073709551616 */

    snprintf(buf, sizeof(buf), "%ld", value);
    httpHeaderAddStrField(hdr, name, buf);
    return value;
}


/* doubles the size of the fields index, starts with INIT_FIELDS_PER_HEADER */
static void
httpHeaderGrow(HttpHeader *hdr)
{
    int new_cap = (hdr->capacity) ? 2*hdr->capacity : INIT_FIELDS_PER_HEADER;
    int new_size = new_cap*sizeof(HttpHeaderField*);

    assert(hdr);

    hdr->fields = (hdr->fields) ? 
	xrealloc(hdr->fields, new_size) :
	xmalloc(new_size);
    memset(hdr->fields+hdr->capacity, 0, (new_cap-hdr->capacity)*sizeof(HttpHeaderField*));
    hdr->capacity = new_cap;
}

static HttpHeaderField *
httpHeaderFieldCreate(const char *name, const char *value)
{
    HttpHeaderField *f = xcalloc(1, sizeof(HttpHeaderField));
    f->name = dupShortString(name);
    f->value = dupShortString(value);
    return f;
}

static void
httpHeaderFieldDestroy(HttpHeaderField *f)
{
    assert(f);
    freeShortString(f->name);
    freeShortString(f->value);
}


const char *
httpHeaderReport()
{
    LOCAL_ARRAY(char, buf, 1024);

    snprintf(buf, sizeof(buf),
	"hdrs: %uld+%uld %s lstr: +%uld-%uld<(%uld=%uld)",
	shortHeadersCount,
	longHeadersCount,
	memPoolReport(shortStrings),
	longStrAllocCount,
	longStrFreeCount,
	longStrHighWaterCount,
	longStrHighWaterSize);
    return buf;
}

/* "short string" routines below are trying to recycle memory for short strings */
static char *
dupShortString(const char *str)
{
    char *buf;
    size_t sz = strlen(str)+1;
    if (sz > shortStrings->obj_size) {
	buf = xmalloc(sz);
	longStrAllocCount++;
	longStrAllocSize += sz;
	if (longStrHighWaterCount < longStrAllocCount - longStrFreeCount)
	    longStrHighWaterCount = longStrAllocCount - longStrFreeCount;
	if (longStrHighWaterSize < longStrAllocSize - longStrFreeSize)
	    longStrHighWaterSize = longStrAllocSize - longStrFreeSize;
    } else
	buf = memPoolGetObj(shortStrings);
    xmemcpy(buf, str, sz); /* includes terminating 0 */
    return buf;
}

static void
freeShortString(char *str)
{
    size_t sz = strlen(str)+1;
    if (sz > shortStrings->obj_size) {
	xfree(str);
	longStrFreeCount++;
	longStrFreeSize += sz;
    } else
	memPoolPutObj(shortStrings, str);
}

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

#include "squid.h"
#include "MemPool.h"
#include "HttpHeader.h"

static const char *KnownSplitableFields[] = {
    "Connection", "Range"
};
/* if you must have KnownSplitableFields empty, set KnownSplitableFieldCount to 0 */
static const int KnownSplitableFieldCount = sizeof(KnownSplitableFields)/sizeof(*KnownSplitableFields);

/* headers accounting */
#define INIT_FIELDS_PER_HEADER 32
static u_num32 shortHeadersCount = 0;
static u_num32 longHeadersCount = 0;

/* recycle bin for short strings */
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
#define conversion_period_name_check(name) assert(!strchr((name), ':'))

HttpHeader *
httpHeaderCreate()
{
    HttpHeader *hdr = xcalloc(1, sizeof(HttpHeader));
    hdr->packed_size = 1; /* we always need one byte for terminating character */
    /* all other members are set to 0 in calloc */

    /* check if pool is ready (no static init in C??) */
    if (!shortStrings)
	shortStrings = memPoolCreate(shortStrPoolCount, shortStrPoolCount/10, shortStrSize, "shortStr");

    return hdr;
}

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

void 
int
httpHeaderParse(HttpHeader *hdr, const char *header_start, const char *header_end)
{
    const char *field_start = buf;

    assert(hdr);
    assert(buf);

    /*
     * first check if there are any chances to parse what we have; @?@ if we are
     * at EOF and no <CRLF> at the end, we might want to parse anyway, however,
     * this is risky because some important headers could be cut off!
     */
    /* Move this check to HttpMsg!!!! @?@ */
    if (!mime_headers_end(buf, size))
	return 0;

    /* commonn format headers are "<name>:[ws]<value>" lines delimited by <CRLF> */
    while (field_start < header_end) {
	const char *field_end = buf + strcspn(buf, "\r\n");
	if (!*field_end) return httpHeaderReset(); /* missing <CRLF> */
	HttpHeaderField f = httpHeaderFieldParseCreate(buf, field_end);
	if (!f) {
	    httpHeaderReset(hdr); /* will remove all fields, etc */
	    return 0;
	}
	httpHeaderAddField(f);
	field_start = field_end;
	/* skip /r/n */
	if (*field_start = '\r') field_start++;
	if (*field_start = '\n') field_start++;
    }
    return 1; /* even if no fields where found, they are optional! */
}

/*
 * puts all the headers into the buffer
 * does not do overflow checking so check with hdr->size first!
 * asserts that exactly hdr->size bytes are put (including terminating 0)
 */
void
httpHeaderPack(HttpHeader *hdr, char *buf)
{
    size_t space_left;
    HttpHeaderPos pos = HttpHeaderInitPos;
    const char *name;
    const char *value;
    assert(hdr && buf);
    space_left = hdr->size;
    /* put all fields one by one */
    while (httpHeaderGetField(hdr, &name, &value, &pos)) {
        size_t add_len = strlen(name) + 2 + strlen(value) + 2;
	assert(space_left > add_len);
	snprintf(buf, space_left, "%s: %s\r\n", name, value);
	buf += add_len;
	space_left -= add_len;
    }
    *buf = '\0';  /* required when no fields are present */
    space_left--;
    assert(!space_left); /* no space left :) */
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
    HttpHeaderField **fp;
    assert(hdr);
    assert(pos >= 0 && pos < hdr->count);

    fp = hdr->fields+pos;
    hdr->packed_size -= httpHeaderFieldBufSize(*fp);
    assert(hdr->packed_size > 0);
    httpHeaderFieldDestroy(*fp);
    *fp = NULL;
}

/* adds a field (appends); may split a well-known field into several ones */
static void
httpHeaderAddField(HttpHeader *hdr, HttpHeaderField *fld)
{
    assert(hdr && fld);

    if (httpHeaderFieldIsList(fld))
	httpHeaderAddListField(hdr, fld); /* splits and adds */
    else
	httpHeaderAddSingleField(hdr, fld); /* just adds */
}

/* adds a field that is guaranteed to be a single (not list) field
 * Warning: This is internal function, never call this directly, 
 *          only for httpHeaderAddField use.
 */
static void
httpHeaderAddSingleField(HttpHeader *hdr, HttpHeaderField *fld)
{
    assert(hdr);
    assert(fld);

    if (hdr->count >= hdr->capacity)
	httpHeaderGrow(hdr);
    hdr->fields[hdr->count++] = fld;
    hdr->packed_size += httpHeaderFieldBufSize(fld);
}

/*
 * Splits list field and appends all entries separately; 
 * Warning: This is internal function, never call this directly, 
 *          only for httpHeaderAddField use.
 */
static void
httpHeaderAddListField(HttpHeader *hdr, HttpHeaderField *fld)
{
    const char *v;
    assert(hdr);
    assert(fld);
    /*
     * Note: assume that somebody already checked that we can split. The danger
     * is in splitting something that is not a list field but contains ','s in
     * its value.
     */
    /* we got a fld.value that is a list of values separated by ',' */
    v = strtok(fld.value, ",");
    httpHeaderAddSingleField(hdr, fld); /* first strtok() did its job! */
    while ((v = strtok(NULL, ",")) {
	/* ltrim and skip empty fields */
	while (isspace(*v) || *v == ',') v++;
	if (*v)
	    httpHeaderAddSingleField(hdr, httpHeaderFieldCreate(fld.name, v));
    }
}

/* adds a string field */
const char *
httpHeaderAddStrField(HttpHeader *hdr, const char *name, const char *value)
{
    assert(hdr);
    assert(name);
    assert(value);

    httpHeaderAddField(hdr, httpHeaderFieldCreate(name, value));
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

/* parses field; returns fresh header field on success and NULL on failure */
static HttpHeaderField *
httpHeaderFieldParseCreate(const char *field_start, const char *field_end)
{
    HttpHeaderField *f = NULL;
    /* note: name_start == field_start */
    const char *name_end = strchr(buf, ':');
    const char *value_start;
    /* note: value_end == field_end */

    if (!name_end || name_end <= field_start || name_end > field_end) 
	return NULL;

    value_start = name_end + 1; /* skip ':' */
    /* skip white space */
    while (value_start < field_end && isspace(*value_start)) 
	value_start++;

    f = xcalloc(1, sizeof(HttpHeaderField));
    f->name = dupShortBuf(field_start, name_end-field_start);
    f->value = dupShortBuf(value_start, field_end-value_start);
    return f;
}

static void
httpHeaderFieldDestroy(HttpHeaderField *f)
{
    assert(f);
    freeShortString(f->name);
    freeShortString(f->value);
}

/*
 * returns the space requred to put a field (and terminating <CRLF>!) into a
 * buffer
 */
static size_t
httpHeaderFieldBufSize(const HttpHeaderField *fld)
{
    return strlen(fld->name)+2+strlen(fld->value)+2;
}

/*
 * returns true if fld.name is a "known" splitable field; 
 * always call this function to check because the detection algortihm may change
 */
static int
httpHeaderFieldIsList(const HttpHeaderField *fld) {
    int i;
    assert(fld);
    /* "onten" should not match "Content"! */
    for (i = 0; i < KnownSplitableFieldCount; ++i)
	if (strcasecmp(KnownSplitableFields[i], fld->name))
	    return 1;
    return 0;
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
    return dupShortBuf(str, strlen(str));
}

static char *
dupShortBuf(const char *str, size_t len)
{
    char *buf;
    size_t sz;
    assert(str);
    assert(len >= 0);
    sz = len + 1;
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
    if (len)
	xmemcpy(buf, str, len); /* may not have terminating 0 */
    buf[len] = '\0'; /* terminate */
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

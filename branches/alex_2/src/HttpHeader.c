/*
 * $Id$
 *
 * DEBUG: section 55    General HTTP Header
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

/* local constants and vars */

/* server cache control */
typedef enum {
    SCC_PUBLIC,
    SCC_PRIVATE,
    SCC_NO_CACHE,
    SCC_NO_STORE,
    SCC_NO_TRANSFORM,
    SCC_MUST_REVALIDATE,
    SCC_PROXY_REVALIDATE,
    SCC_MAX_AGE,
    SCC_OTHER,
    SCC_ENUM_END
} http_scc_t;

typedef struct {
    const char *name;
    int len;
    int id;
    struct { int test1; int test2; } dummy;
} field_attrs_t;

static field_attrs_t HdrFieldAttrs[] = {
    { "Accept:",             7, HDR_ACCEPT },
    { "Age:",                4, HDR_AGE },
    { "Cache-Control:",     14, HDR_CACHE_CONTROL },
    { "Content-Length:",    15, HDR_CONTENT_LENGTH },
    { "Content-MD5:",       12, HDR_CONTENT_MD5 },
    { "Content-Type:",      13, HDR_CONTENT_TYPE },
    { "Date:",               5, HDR_DATE },
    { "Etag:",               5, HDR_ETAG },
    { "Expires:",            8, HDR_EXPIRES },
    { "Host:",               5, HDR_HOST },
    { "If-Modified-Since:", 18, HDR_IMS },
    { "Last-Modified:",     14, HDR_LAST_MODIFIED },
    { "Max-Forwards:",      13, HDR_MAX_FORWARDS },
    { "Public:",             7, HDR_PUBLIC },
    { "Retry-After:",       12, HDR_RETRY_AFTER },
    { "Set-Cookie:",        11, HDR_SET_COOKIE },
    { "Upgrade:",            8, HDR_UPGRADE },
    { "Warning:",            8, HDR_WARNING },
    { "Proxy-Connection:",  17, HDR_PROXY_KEEPALIVE }, /* special */
    { "Other",               6, HDR_OTHER },   /* @?@ check this! */
    { "NONE",                5, HDR_ENUM_END}  /* @?@ check this! */
};

static field_attrs_t SccFieldAttrs[] = {
    { "public",            6, SCC_PUBLIC },
    { "private",           7, SCC_PRIVATE },
    { "no-cache",          8, SCC_NO_CACHE },
    { "no-store",          8, SCC_NO_STORE },
    { "no-transform",     12, SCC_NO_TRANSFORM },
    { "must-revalidate",  15, SCC_MUST_REVALIDATE },
    { "proxy-revalidate", 16, SCC_PROXY_REVALIDATE },
    { "max-age",           7, SCC_MAX_AGE },
};

static int ReplyHeadersMask = 0; /* set run-time using ReplyHeaders */
static http_hdr_type ReplyHeaders[] = {
    HDR_ACCEPT, HDR_AGE, HDR_CACHE_CONTROL, HDR_CONTENT_LENGTH,
    HDR_CONTENT_MD5,  HDR_CONTENT_TYPE, HDR_DATE, HDR_ETAG, HDR_EXPIRES,
    HDR_LAST_MODIFIED, HDR_MAX_FORWARDS, HDR_PUBLIC, HDR_RETRY_AFTER,
    HDR_SET_COOKIE, HDR_UPGRADE, HDR_WARNING, HDR_PROXY_KEEPALIVE, HDR_OTHER
};

static int RequestHeadersMask = 0; /* set run-time using RequestHeaders */
static http_hdr_type RequestHeaders[] = {
    HDR_OTHER
};

static const char *KnownSplitableFields[] = {
    "Connection", "Range"
};
/* if you must have KnownSplitableFields empty, set KnownSplitableFieldCount to 0 */
static const int KnownSplitableFieldCount = sizeof(KnownSplitableFields)/sizeof(*KnownSplitableFields);

/* headers accounting */
#define INIT_FIELDS_PER_HEADER 32
static u_num32 shortHeadersCount = 0;
static u_num32 longHeadersCount = 0;

typedef struct {
    const char *label;
    int parsed;
    int misc[HDR_ENUM_END];
} HttpHeaderStats;

#if 0 /* not used, add them later @?@ */
static struct {
    int parsed;
    int misc[HDR_MISC_END];
    int cc[SCC_ENUM_END];
} ReplyHeaderStats;


/* various strings to use in reports */
static char *HttpServerCCStr[] =
{
    "public",
    "private",
    "no-cache",
    "no-store",
    "no-transform",
    "must-revalidate",
    "proxy-revalidate",
    "max-age",
    "NONE"
};

static char *HttpHdrMiscStr[] =
{
    "Accept",
    "Age",
    "Content-Length",
    "Content-MD5",
    "Content-Type",
    "Date",
    "Etag",
    "Expires",
    "Host",
    "If-Modified-Since",
    "Last-Modified",
    "Max-Forwards",
    "Public",
    "Retry-After",
    "Set-Cookie",
    "Upgrade",
    "Warning",
    "NONE"
};
#endif /* if 0 */

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
static void httpHeaderAddField(HttpHeader *hdr, HttpHeaderField *fld);
static void httpHeaderAddSingleField(HttpHeader *hdr, HttpHeaderField *fld);
static void httpHeaderAddListField(HttpHeader *hdr, HttpHeaderField *fld);
static void httpHeaderCountField(HttpHeader *hdr, HttpHeaderField *fld);
static void httpHeaderCountSCCField(HttpHeader *hdr, HttpHeaderField *fld);
static int httpHeaderFindFieldType(HttpHeaderField *fld, const field_attrs_t *attrs, int end, int mask);
static HttpHeaderField *httpHeaderFieldCreate(const char *name, const char *value);
static HttpHeaderField *httpHeaderFieldParseCreate(const char *field_start, const char *field_end);
static void httpHeaderFieldDestroy(HttpHeaderField *f);
static size_t httpHeaderFieldBufSize(const HttpHeaderField *fld);
static int httpHeaderFieldIsList(const HttpHeaderField *fld);
static void httpHeaderStoreAReport(StoreEntry *e, HttpHeaderStats *stats);

static char *dupShortString(const char *str);
static char *dupShortBuf(const char *str, size_t len);
static void freeShortString(char *str);

/* delete this when everybody remembers that ':' is not a part of a name */
#define conversion_period_name_check(name) assert(!strchr((name), ':'))

HttpHeader *
httpHeaderCreate()
{
    HttpHeader *hdr = xmalloc(sizeof(HttpHeader));
    httpHeaderInit(hdr);
    return hdr;
}


/* "create" for non-alloc objects; also used by real Create to avoid code duplication */
void
httpHeaderInit(HttpHeader *hdr)
{
    assert(hdr);
    memset(hdr, 0, sizeof(*hdr));

    hdr->packed_size = 1; /* we always need one byte for terminating character */

    /* check if pool is ready (no static init in C??) @?@ */
    if (!shortStrings)
	shortStrings = memPoolCreate(shortStrPoolCount, shortStrPoolCount/10, shortStrSize, "shortStr");
}

void
httpHeaderDestroy(HttpHeader *hdr)
{
    httpHeaderClean(hdr);
    xfree(hdr);
}

void
httpHeaderClean(HttpHeader *hdr)
{
    HttpHeaderPos pos = HttpHeaderInitPos;
    HttpHeaderField *f;

    assert(hdr);

    if (hdr->capacity > INIT_FIELDS_PER_HEADER)
	longHeadersCount++;
    else
	shortHeadersCount++;

    while ((f = httpHeaderGetField(hdr, 0, 0, &pos)))
	httpHeaderFieldDestroy(f);
}

/* just handy in parsing: resets and returns false */
static int
httpHeaderReset(HttpHeader *hdr) {
    httpHeaderClean(hdr);
    httpHeaderInit(hdr);
    return 0;
}
 
int
httpHeaderParse(HttpHeader *hdr, const char *header_start, const char *header_end)
{
    const char *field_start = header_start;
    HttpHeaderField *f;

    assert(hdr);
    assert(header_start && header_end);
    /* commonn format headers are "<name>:[ws]<value>" lines delimited by <CRLF> */
    while (field_start < header_end) {
	const char *field_end = field_start + strcspn(field_start, "\r\n");
	if (!*field_end) 
	    return httpHeaderReset(hdr); /* missing <CRLF> */
	if (!(f = httpHeaderFieldParseCreate(field_start, field_end)));
	    return httpHeaderReset(hdr);
	httpHeaderAddField(hdr, f);
	field_start = field_end;
	/* skip /r/n */
	if (*field_start == '\r') field_start++;
	if (*field_start == '\n') field_start++;
    }
    return 1; /* even if no fields where found, they could be optional! */
}

/*
 * packs all the fields into the buffer
 * does not do overflow checking so check with hdr->packed_size first!
 * asserts that exactly hdr->packed_size bytes are put (including terminating 0)
 */
int
httpHeaderPackInto(const HttpHeader *hdr, char *buf)
{
    size_t space_left;
    HttpHeaderPos pos = HttpHeaderInitPos;
    const char *name;
    const char *value;
    assert(hdr && buf);
    space_left = hdr->packed_size;
    /* pack all fields one by one */
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
    return hdr->packed_size;
}

/* swaps out headers */
void
httpHeaderSwap(HttpHeader *hdr, StoreEntry *e) {
    HttpHeaderPos pos = HttpHeaderInitPos;
    const char *name;
    const char *value;
    assert(hdr && e);
    /* swap out all fields one by one */
    while (httpHeaderGetField(hdr, &name, &value, &pos)) {
	storeAppendPrintf(e, "%s: %s\r\n", name, value);
    }
}

const char *
httpHeaderGetStr(const HttpHeader *hdr, const char *name, HttpHeaderPos *pos)
{
    const char *n;
    const char *v;

    assert(hdr);
    assert(name);
    conversion_period_name_check(name);

    while(httpHeaderGetField(hdr, &n, &v, pos))
    	if (strcasecmp(name, n) == 0)
    	    return v;
    return NULL;
}

long 
httpHeaderGetInt(const HttpHeader *hdr, const char *name, HttpHeaderPos *pos)
{
    const char *str = httpHeaderGetStr(hdr, name, pos);
    return str ? atol(str) : -1;
}


/* rfc1123 */
time_t
httpHeaderGetDate(const HttpHeader *hdr, const char *name, HttpHeaderPos *pos) {
    const char *str = httpHeaderGetStr(hdr, name, pos);
    return str ? parse_rfc1123(str) : -1;
}

HttpHeaderField *
httpHeaderGetField(const HttpHeader *hdr, const char **name, const char **value, HttpHeaderPos *pos)
{
    /* we do not want to care about '!pos' in the loop: */
    HttpHeaderPos p = HttpHeaderInitPos;
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
    HttpHeaderPos pos = HttpHeaderInitPos;

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
    /* accounting */
    httpHeaderCountField(hdr, fld);
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
    v = strtok(fld->value, ",");
    httpHeaderAddSingleField(hdr, fld); /* first strtok() did its job! */
    while ((v = strtok(NULL, ","))) {
	/* ltrim and skip empty fields */
	while (isspace(*v) || *v == ',') v++;
	if (*v)
	    httpHeaderAddSingleField(hdr, httpHeaderFieldCreate(fld->name, v));
    }
}

/* adds a string field */
const char *
httpHeaderAddStr(HttpHeader *hdr, const char *name, const char *value)
{
    assert(hdr);
    assert(name);
    assert(value);

    httpHeaderAddField(hdr, httpHeaderFieldCreate(name, value));
    return value;
}

long
httpHeaderAddInt(HttpHeader *hdr, const char *name, long value) {
    static char buf[32]; /* 2^64 = 18446744073709551616 */

    snprintf(buf, sizeof(buf), "%ld", value);
    httpHeaderAddStr(hdr, name, buf);
    return value;
}

/* uses mkrfc1123 */
time_t
httpHeaderAddDate(HttpHeader *hdr, const char *name, time_t value)
{
    httpHeaderAddStr(hdr, name, mkrfc1123(value));
    return value;
}

int
httpHeaderGetContentLength(const HttpHeader *hdr)
{
    /* do we need any special processing here? @?@ */
    return httpHeaderGetInt(hdr, "Content-Length", NULL);
}

time_t
httpHeaderGetExpires(const HttpHeader *hdr) {
    time_t value = -1;

    /* The max-age directive takes priority over Expires, check it first */
    if (EBIT_TEST(hdr->scc_mask, SCC_MAX_AGE)) {
	HttpHeaderPos pos = HttpHeaderInitPos;
	const char *max_age_str;
	while ((max_age_str = httpHeaderGetStr(hdr, "Cache-Control", &pos))) {
	    if (!strncasecmp(max_age_str, "max-age", 7)) {
		/* skip white space */
		while (*max_age_str && isspace(*max_age_str))
		    max_age_str++;
		if (*max_age_str == '=') {
		    time_t max_age = (time_t)atoi(++max_age_str);
		    if (max_age > 0)
			value = squid_curtime + max_age;
		}
	    }
	}
    }
    if (value < 0)
	value = httpHeaderGetDate(hdr, "Expires", NULL);
    /*
     * The HTTP/1.0 specs says that robust implementations should consider bad
     * or malformed Expires header as equivalent to "expires immediately."
     */
    return (value < 0) ? squid_curtime : value;
}

/* updates masks and stats for a field */
static void
httpHeaderCountField(HttpHeader *hdr, HttpHeaderField *fld)
{
    /* add Req/Pep detection here @?@ */
    int type = httpHeaderFindFieldType(fld,
	HdrFieldAttrs, HDR_ENUM_END,
	(1) ? ReplyHeadersMask : RequestHeadersMask);
    /* exception */
    if (type == HDR_PROXY_KEEPALIVE && strcasecmp("Keep-Alive", fld->value))
	type = -1;
    if (type < 0)
	type = HDR_OTHER;
    /* update mask */
    EBIT_SET(hdr->field_mask, type);
    /* @?@ update stats for req/resp:type @?@ */
    HdrFieldAttrs[type].dummy.test1++;
    /* process scc @?@ check if we need to do that for requests or not */
    if (1 && type == HDR_CACHE_CONTROL)
	httpHeaderCountSCCField(hdr, fld);
}

/* updates scc mask and stats for an scc field */
static void
httpHeaderCountSCCField(HttpHeader *hdr, HttpHeaderField *fld)
{
    int type = httpHeaderFindFieldType(fld,
	SccFieldAttrs, SCC_ENUM_END, -1);
    if (type < 0)
	type = SCC_OTHER;
    /* update mask */
    EBIT_SET(hdr->scc_mask, type);
    /* @?@ update stats for scc @?@ */
    SccFieldAttrs[type].dummy.test1++;
}

static int
httpHeaderFindFieldType(HttpHeaderField *fld, const field_attrs_t *attrs, int end, int mask) {
    int i;
    for (i = 0; i < end; ++i)
	if (mask < 0 || EBIT_TEST(mask, i))
	    if (!strncasecmp(fld->name, attrs[i].name, attrs[i].len))
		return i;
    return -1;
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
    const char *name_end = strchr(field_start, ':');
    const char *value_start;
    /* note: value_end == field_end */

    if (!name_end || name_end <= field_start || name_end > field_end) 
	return NULL;

    value_start = name_end + 1; /* skip ':' */
    /* skip white space */
    while (value_start < field_end && isspace(*value_start)) 
	value_start++;

    /* cut off "; parameter" from Content-Type @?@ why? */
    if (!strncasecmp(field_start, "Content-Type:", 14)) {
	const int l = strcspn(value_start, ";\t ");
	if (l > 0)
	    field_end = value_start + l;
    }

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
    xfree(f);
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

void
httpHeaderStoreReport(StoreEntry *e)
{
    assert(e);

    httpHeaderStoreRepReport(e);
    httpHeaderStoreReqReport(e);

    /* low level totals; reformat this? @?@ */
    storeAppendPrintf(e,
	"hdrs totals: %uld+%uld %s lstr: +%uld-%uld<(%uld=%uld)\n",
	shortHeadersCount,
	longHeadersCount,
	memPoolReport(shortStrings),
	longStrAllocCount,
	longStrFreeCount,
	longStrHighWaterCount,
	longStrHighWaterSize);
}

void
httpHeaderStoreRepReport(StoreEntry *e)
{
    assert(e);
#if 0 /* implement this */
    httpHeaderStoreAReport(e, &ReplyHeaderStats);
    for (i = SCC_PUBLIC; i < SCC_ENUM_END; i++)
	storeAppendPrintf(entry, "Cache-Control %s: %d\n",
	    HttpServerCCStr[i],
	    ReplyHeaderStats.cc[i]);
#endif
}

void
httpHeaderStoreReqReport(StoreEntry *e)
{
    assert(e);
#if 0 /* implement this */
    httpHeaderStoreAReport(e, &RequestHeaderStats);
#endif
}

static void
httpHeaderStoreAReport(StoreEntry *e, HttpHeaderStats *stats)
{
    assert(e);
    assert(stats);
    assert(0);
#if 0 /* implement this using stats pointer @?@ */
    http_server_cc_t i;
    http_hdr_misc_t j;
    storeAppendPrintf(entry, "HTTP Reply Headers:\n");
    storeAppendPrintf(entry, "       Headers parsed: %d\n",
	ReplyHeaderStats.parsed);
    for (j = HDR_AGE; j < HDR_MISC_END; j++)
	storeAppendPrintf(entry, "%21.21s: %d\n",
	    HttpHdrMiscStr[j],
	    ReplyHeaderStats.misc[j]);
#endif
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

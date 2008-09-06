
/*
 * $Id$
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

#include "squid.h"

/*
 * On naming conventions:
 * 
 * HTTP/1.1 defines message-header as 
 * 
 * message-header = field-name ":" [ field-value ] CRLF
 * field-name     = token
 * field-value    = *( field-content | LWS )
 * 
 * HTTP/1.1 does not give a name name a group of all message-headers in a message.
 * Squid 1.1 seems to refer to that group _plus_ start-line as "headers".
 * 
 * HttpHeader is an object that represents all message-headers in a message.
 * HttpHeader does not manage start-line.
 * 
 * HttpHeader is implemented as a collection of header "entries".
 * An entry is a (field_id, field_name, field_value) triplet.
 */


/*
 * local constants and vars
 */

/*
 * local routines
 */

#define assert_eid(id) assert((id) < HDR_ENUM_END)

static void httpHeaderNoteParsedEntry(http_hdr_type id, String value, int error);

static void httpHeaderStatDump(const HttpHeaderStat * hs, StoreEntry * e);

MemPool * pool_http_reply = NULL;
MemPool * pool_http_hdr_cc = NULL;
MemPool * pool_http_hdr_range_spec = NULL;
MemPool * pool_http_hdr_range = NULL;
MemPool * pool_http_hdr_cont_range = NULL;

/*
 * Module initialization routines
 */

void
httpHeaderInitMem(void)
{
    pool_http_reply = memPoolCreate("HttpReply", sizeof(HttpReply));
    pool_http_hdr_cc = memPoolCreate("HttpHdrCc", sizeof(HttpHdrCc));
    pool_http_hdr_range_spec = memPoolCreate("HttpHdrRangeSpec", sizeof(HttpHdrRangeSpec));
    pool_http_hdr_range = memPoolCreate("HttpHdrRange", sizeof(HttpHdrRange));
    pool_http_hdr_cont_range = memPoolCreate("HttpHdrContRange", sizeof(HttpHdrContRange));
}

void
httpHeaderInitModule(void)
{
    /* Setup the libhttp/ header stuff */
    httpHeaderInitLibrary();

    /* init dependent modules */
    httpHdrCcInitModule();
    /* register with cache manager */
    cachemgrRegister("http_headers",
	"HTTP Header Statistics", httpHeaderStoreReport, 0, 1);
}

void
httpHeaderCleanModule(void)
{
    httpHeaderDestroyFieldsInfo(Headers, HDR_ENUM_END);
    Headers = NULL;
    httpHdrCcCleanModule();
}

/*
 * HttpHeader Implementation
 */

static void
httpHeaderRepack(HttpHeader * hdr)
{
    HttpHeaderPos dp = HttpHeaderInitPos;
    HttpHeaderPos pos = HttpHeaderInitPos;

    /* XXX breaks layering for now! ie, getting grubby fingers in without httpHeaderEntryGet() */
    dp = 0;
    pos = 0;
    while (dp < hdr->entries.count) {
	for (; dp < hdr->entries.count && hdr->entries.items[dp] == NULL; dp++);
	if (dp >= hdr->entries.count)
	    break;
	hdr->entries.items[pos] = hdr->entries.items[dp];
	if (dp != pos)
	    hdr->entries.items[dp] = NULL;
	pos++;
	dp++;
    }
    arrayShrink(&hdr->entries, pos);
}

/* use fresh entries to replace old ones */
void
httpHeaderUpdate(HttpHeader * old, const HttpHeader * fresh, const HttpHeaderMask * denied_mask)
{
    const HttpHeaderEntry *e;
    HttpHeaderPos pos = HttpHeaderInitPos;

    assert(old && fresh);
    assert(old != fresh);
    debug(55, 7) ("updating hdr: %p <- %p\n", old, fresh);

    while ((e = httpHeaderGetEntry(fresh, &pos))) {
	/* deny bad guys (ok to check for HDR_OTHER) here */
	if (denied_mask && CBIT_TEST(*denied_mask, e->id))
	    continue;
	if (e->id != HDR_OTHER)
	    httpHeaderDelById(old, e->id);
	else
	    httpHeaderDelByName(old, strBuf(e->name));
    }
    pos = HttpHeaderInitPos;
    while ((e = httpHeaderGetEntry(fresh, &pos))) {
	/* deny bad guys (ok to check for HDR_OTHER) here */
	if (denied_mask && CBIT_TEST(*denied_mask, e->id))
	    continue;
	httpHeaderAddClone(old, e);
    }

    /* And now, repack the array to "fill in the holes" */
    httpHeaderRepack(old);
}

/* packs all the entries using supplied packer */
void
httpHeaderPackInto(const HttpHeader * hdr, Packer * p)
{
    HttpHeaderPos pos = HttpHeaderInitPos;
    const HttpHeaderEntry *e;
    assert(hdr && p);
    debug(55, 7) ("packing hdr: (%p)\n", hdr);
    /* pack all entries one by one */
    while ((e = httpHeaderGetEntry(hdr, &pos)))
	httpHeaderEntryPackInto(e, p);
}

/*
 * Refreshes the header mask. Useful after httpHeaderDelAt constructs
 */
void
httpHeaderRefreshMask(HttpHeader * hdr)
{
    HttpHeaderPos pos = HttpHeaderInitPos;
    HttpHeaderEntry *e;
    httpHeaderMaskInit(&hdr->mask, 0);
    debug(55, 7) ("refreshing the mask in hdr %p\n", hdr);
    while ((e = httpHeaderGetEntry(hdr, &pos))) {
	CBIT_SET(hdr->mask, e->id);
    }
}



/* return a list of entries with the same id separated by ',' and ws */
String
httpHeaderGetList(const HttpHeader * hdr, http_hdr_type id)
{
    String s = StringNull;
    HttpHeaderEntry *e;
    HttpHeaderPos pos = HttpHeaderInitPos;
    debug(55, 6) ("%p: joining for id %d\n", hdr, id);
    /* only fields from ListHeaders array can be "listed" */
    assert(CBIT_TEST(ListHeadersMask, id));
    if (!CBIT_TEST(hdr->mask, id))
	return s;
    while ((e = httpHeaderGetEntry(hdr, &pos))) {
	if (e->id == id)
	    strListAdd(&s, strBuf(e->value), ',');
    }
    /*
     * note: we might get an empty (len==0) string if there was an "empty"
     * header; we must not get a NULL string though.
     */
    assert(strBuf(s));
    /* temporary warning: remove it! @?@ @?@ @?@ */
    if (!strLen(s))
	debug(55, 3) ("empty list header: %s (%d)\n", strBuf(Headers[id].name), id);
    debug(55, 6) ("%p: joined for id %d: %s\n", hdr, id, strBuf(s));
    return s;
}

/* return a string or list of entries with the same id separated by ',' and ws */
String
httpHeaderGetStrOrList(const HttpHeader * hdr, http_hdr_type id)
{
    HttpHeaderEntry *e;

    if (CBIT_TEST(ListHeadersMask, id))
	return httpHeaderGetList(hdr, id);
    if ((e = httpHeaderFindEntry(hdr, id))) {
	String s;
	stringLimitInit(&s, strBuf(e->value), strLen(e->value));
	return s;
    }
    return StringNull;
}

/*
 * Returns the value of the specified header.
 */
String
httpHeaderGetByName(const HttpHeader * hdr, const char *name)
{
    http_hdr_type id;
    HttpHeaderPos pos = HttpHeaderInitPos;
    HttpHeaderEntry *e;
    String result = StringNull;

    assert(hdr);
    assert(name);

    /* First try the quick path */
    id = httpHeaderIdByNameDef(name, strlen(name));
    if (id != -1)
	return httpHeaderGetStrOrList(hdr, id);

    /* Sorry, an unknown header name. Do linear search */
    while ((e = httpHeaderGetEntry(hdr, &pos))) {
	if (e->id == HDR_OTHER && strCaseCmp(e->name, name) == 0) {
	    strListAdd(&result, strBuf(e->value), ',');
	}
    }
    return result;
}

/*
 * returns a pointer to a specified entry if any 
 * note that we return one entry so it does not make much sense to ask for
 * "list" headers
 */
String
httpHeaderGetByNameListMember(const HttpHeader * hdr, const char *name, const char *member, const char separator)
{
    String result = StringNull;
    String header;
    const char *pos = NULL;
    const char *item;
    int ilen;
    int mlen = strlen(member);

    assert(hdr);
    assert(name);

    header = httpHeaderGetByName(hdr, name);

    while (strListGetItem(&header, separator, &item, &ilen, &pos)) {
	if (strncmp(item, member, mlen) == 0 && item[mlen] == '=') {
	    stringAppend(&result, item + mlen + 1, ilen - mlen - 1);
	    break;
	}
    }
    stringClean(&header);
    return result;
}

/*
 * returns a the value of the specified list member, if any.
 */
String
httpHeaderGetListMember(const HttpHeader * hdr, http_hdr_type id, const char *member, const char separator)
{
    String result = StringNull;
    String header;
    const char *pos = NULL;
    const char *item;
    int ilen;
    int mlen = strlen(member);

    assert(hdr);
    assert_eid(id);

    header = httpHeaderGetStrOrList(hdr, id);

    while (strListGetItem(&header, separator, &item, &ilen, &pos)) {
	if (strncmp(item, member, mlen) == 0 && item[mlen] == '=') {
	    stringAppend(&result, item + mlen + 1, ilen - mlen - 1);
	    break;
	}
    }
    stringClean(&header);
    return result;
}

/* test if a field is present */
int
httpHeaderHas(const HttpHeader * hdr, http_hdr_type id)
{
    assert(hdr);
    assert_eid(id);
    assert(id != HDR_OTHER);
    debug(55, 7) ("%p lookup for %d\n", hdr, id);
    return CBIT_TEST(hdr->mask, id);
}

void
httpHeaderPutInt(HttpHeader * hdr, http_hdr_type id, int number)
{
    assert_eid(id);
    assert(Headers[id].type == ftInt);	/* must be of an appropriate type */
    assert(number >= 0);
    httpHeaderAddEntryStr(hdr, id, NULL, xitoa(number));
}

void
httpHeaderPutSize(HttpHeader * hdr, http_hdr_type id, squid_off_t number)
{
    char size[64];
    assert_eid(id);
    assert(Headers[id].type == ftSize);		/* must be of an appropriate type */
    assert(number >= 0);
    snprintf(size, sizeof(size), "%" PRINTF_OFF_T, number);
    httpHeaderAddEntryStr(hdr, id, NULL, size);
}

void
httpHeaderPutTime(HttpHeader * hdr, http_hdr_type id, time_t htime)
{
    assert_eid(id);
    assert(Headers[id].type == ftDate_1123);	/* must be of an appropriate type */
    assert(htime >= 0);
    httpHeaderAddEntryStr(hdr, id, NULL, mkrfc1123(htime));
}

void
httpHeaderInsertTime(HttpHeader * hdr, int pos, http_hdr_type id, time_t htime)
{
    assert_eid(id);
    assert(Headers[id].type == ftDate_1123);	/* must be of an appropriate type */
    assert(htime >= 0);
    httpHeaderInsertEntryStr(hdr, pos, id, NULL, mkrfc1123(htime));
}

void
httpHeaderPutStr(HttpHeader * hdr, http_hdr_type id, const char *str)
{
    assert_eid(id);
    assert(Headers[id].type == ftStr);	/* must be of an appropriate type */
    assert(str);
    httpHeaderAddEntryStr(hdr, id, NULL, str);
}

void
httpHeaderPutAuth(HttpHeader * hdr, const char *auth_scheme, const char *realm)
{
    assert(hdr && auth_scheme && realm);
    httpHeaderPutStrf(hdr, HDR_WWW_AUTHENTICATE, "%s realm=\"%s\"", auth_scheme, realm);
}

void
httpHeaderPutCc(HttpHeader * hdr, const HttpHdrCc * cc)
{
    MemBuf mb;
    Packer p;
    assert(hdr && cc);
    /* remove old directives if any */
    httpHeaderDelById(hdr, HDR_CACHE_CONTROL);
    /* pack into mb */
    memBufDefInit(&mb);
    packerToMemInit(&p, &mb);
    httpHdrCcPackInto(cc, &p);
    /* put */
    httpHeaderAddEntryStr(hdr, HDR_CACHE_CONTROL, NULL, mb.buf);
    /* cleanup */
    packerClean(&p);
    memBufClean(&mb);
}

void
httpHeaderPutContRange(HttpHeader * hdr, const HttpHdrContRange * cr)
{
    MemBuf mb;
    Packer p;
    assert(hdr && cr);
    /* remove old directives if any */
    httpHeaderDelById(hdr, HDR_CONTENT_RANGE);
    /* pack into mb */
    memBufDefInit(&mb);
    packerToMemInit(&p, &mb);
    httpHdrContRangePackInto(cr, &p);
    /* put */
    httpHeaderAddEntryStr(hdr, HDR_CONTENT_RANGE, NULL, mb.buf);
    /* cleanup */
    packerClean(&p);
    memBufClean(&mb);
}

void
httpHeaderPutRange(HttpHeader * hdr, const HttpHdrRange * range)
{
    MemBuf mb;
    Packer p;
    assert(hdr && range);
    /* remove old directives if any */
    httpHeaderDelById(hdr, HDR_RANGE);
    /* pack into mb */
    memBufDefInit(&mb);
    packerToMemInit(&p, &mb);
    httpHdrRangePackInto(range, &p);
    /* put */
    httpHeaderAddEntryStr(hdr, HDR_RANGE, NULL, mb.buf);
    /* cleanup */
    packerClean(&p);
    memBufClean(&mb);
}

/* add extension header (these fields are not parsed/analyzed/joined, etc.) */
void
httpHeaderPutExt(HttpHeader * hdr, const char *name, const char *value)
{
    assert(name && value);
    debug(55, 8) ("%p adds ext entry '%s: %s'\n", hdr, name, value);
    httpHeaderAddEntryStr(hdr, HDR_OTHER, name, value);
}

int
httpHeaderGetInt(const HttpHeader * hdr, http_hdr_type id)
{
    HttpHeaderEntry *e;
    int value = -1;
    int ok;
    assert_eid(id);
    assert(Headers[id].type == ftInt);	/* must be of an appropriate type */
    if ((e = httpHeaderFindEntry(hdr, id))) {
	ok = httpHeaderParseInt(strBuf(e->value), &value);
	httpHeaderNoteParsedEntry(e->id, e->value, !ok);
    }
    return value;
}

squid_off_t
httpHeaderGetSize(const HttpHeader * hdr, http_hdr_type id)
{
    HttpHeaderEntry *e;
    squid_off_t value = -1;
    int ok;
    assert_eid(id);
    assert(Headers[id].type == ftSize);		/* must be of an appropriate type */
    if ((e = httpHeaderFindEntry(hdr, id))) {
	ok = httpHeaderParseSize(strBuf(e->value), &value);
	httpHeaderNoteParsedEntry(e->id, e->value, !ok);
    }
    return value;
}

time_t
httpHeaderGetTime(const HttpHeader * hdr, http_hdr_type id)
{
    HttpHeaderEntry *e;
    time_t value = -1;
    assert_eid(id);
    assert(Headers[id].type == ftDate_1123);	/* must be of an appropriate type */
    if ((e = httpHeaderFindEntry(hdr, id))) {
	value = parse_rfc1123(strBuf(e->value), strLen(e->value));
	httpHeaderNoteParsedEntry(e->id, e->value, value < 0);
    }
    return value;
}

/* sync with httpHeaderGetLastStr */
const char *
httpHeaderGetStr(const HttpHeader * hdr, http_hdr_type id)
{
    HttpHeaderEntry *e;
    assert_eid(id);
    assert(Headers[id].type == ftStr);	/* must be of an appropriate type */
    if ((e = httpHeaderFindEntry(hdr, id))) {
	httpHeaderNoteParsedEntry(e->id, e->value, 0);	/* no errors are possible */
	return strBuf(e->value);
    }
    return NULL;
}

/* unusual */
const char *
httpHeaderGetLastStr(const HttpHeader * hdr, http_hdr_type id)
{
    HttpHeaderEntry *e;
    assert_eid(id);
    assert(Headers[id].type == ftStr);	/* must be of an appropriate type */
    if ((e = httpHeaderFindLastEntry(hdr, id))) {
	httpHeaderNoteParsedEntry(e->id, e->value, 0);	/* no errors are possible */
	return strBuf(e->value);
    }
    return NULL;
}

HttpHdrCc *
httpHeaderGetCc(const HttpHeader * hdr)
{
    HttpHdrCc *cc;
    String s;
    if (!CBIT_TEST(hdr->mask, HDR_CACHE_CONTROL))
	return NULL;
    s = httpHeaderGetList(hdr, HDR_CACHE_CONTROL);
    cc = httpHdrCcParseCreate(&s);
    HttpHeaderStats[hdr->owner].ccParsedCount++;
    if (cc)
	httpHdrCcUpdateStats(cc, &HttpHeaderStats[hdr->owner].ccTypeDistr);
    httpHeaderNoteParsedEntry(HDR_CACHE_CONTROL, s, !cc);
    stringClean(&s);
    return cc;
}

HttpHdrRange *
httpHeaderGetRange(const HttpHeader * hdr)
{
    HttpHdrRange *r = NULL;
    HttpHeaderEntry *e;
    /* some clients will send "Request-Range" _and_ *matching* "Range"
     * who knows, some clients might send Request-Range only;
     * this "if" should work correctly in both cases;
     * hopefully no clients send mismatched headers! */
    if ((e = httpHeaderFindEntry(hdr, HDR_RANGE)) ||
	(e = httpHeaderFindEntry(hdr, HDR_REQUEST_RANGE))) {
	r = httpHdrRangeParseCreate(&e->value);
	httpHeaderNoteParsedEntry(e->id, e->value, !r);
    }
    return r;
}

HttpHdrContRange *
httpHeaderGetContRange(const HttpHeader * hdr)
{
    HttpHdrContRange *cr = NULL;
    HttpHeaderEntry *e;
    if ((e = httpHeaderFindEntry(hdr, HDR_CONTENT_RANGE))) {
	cr = httpHdrContRangeParseCreate(strBuf(e->value));
	httpHeaderNoteParsedEntry(e->id, e->value, !cr);
    }
    return cr;
}

const char *
httpHeaderGetAuth(const HttpHeader * hdr, http_hdr_type id, const char *auth_scheme)
{
    const char *field;
    int l;
    assert(hdr && auth_scheme);
    field = httpHeaderGetStr(hdr, id);
    if (!field)			/* no authorization field */
	return NULL;
    l = strlen(auth_scheme);
    if (!l || strncasecmp(field, auth_scheme, l))	/* wrong scheme */
	return NULL;
    field += l;
    if (!xisspace(*field))	/* wrong scheme */
	return NULL;
    /* skip white space */
    field += xcountws(field);
    if (!*field)		/* no authorization cookie */
	return NULL;
    return base64_decode(field);
}

TimeOrTag
httpHeaderGetTimeOrTag(const HttpHeader * hdr, http_hdr_type id)
{
    TimeOrTag tot;
    HttpHeaderEntry *e;
    assert(Headers[id].type == ftDate_1123_or_ETag);	/* must be of an appropriate type */
    memset(&tot, 0, sizeof(tot));
    if ((e = httpHeaderFindEntry(hdr, id))) {
	const char *str = strBuf(e->value);
	/* try as an ETag */
	if (*str == '"' || (str[0] == 'W' && str[1] == '/')) {
	    tot.tag = str;
	    tot.time = -1;
	    tot.valid = 1;
	} else {
	    /* or maybe it is time? */
	    tot.time = parse_rfc1123(str, strLen(e->value));
	    if (tot.time >= 0)
		tot.valid = 1;
	    tot.tag = NULL;
	}
    } else {
	tot.time = -1;
    }
    return tot;
}

/*
 * HttpHeaderEntry
 */

void
httpHeaderEntryPackInto(const HttpHeaderEntry * e, Packer * p)
{
    assert(e && p);
    packerAppend(p, strBuf(e->name), strLen(e->name));
    packerAppend(p, ": ", 2);
    packerAppend(p, strBuf(e->value), strLen(e->value));
    packerAppend(p, "\r\n", 2);
}

static void
httpHeaderNoteParsedEntry(http_hdr_type id, String context, int error)
{
    Headers[id].stat.parsCount++;
    if (error) {
	Headers[id].stat.errCount++;
	debug(55, 2) ("cannot parse hdr field: '%s: %s'\n",
	    strBuf(Headers[id].name), strBuf(context));
    }
}

/*
 * Reports
 */

/* tmp variable used to pass stat info to dumpers */
extern const HttpHeaderStat *dump_stat;		/* argh! */
const HttpHeaderStat *dump_stat = NULL;

static void
httpHeaderFieldStatDumper(StoreEntry * sentry, int idx, double val, double size, int count)
{
    const int id = (int) val;
    const int valid_id = id >= 0 && id < HDR_ENUM_END;
    const char *name = valid_id ? strBuf(Headers[id].name) : "INVALID";
    int visible = count > 0;
    /* for entries with zero count, list only those that belong to current type of message */
    if (!visible && valid_id && dump_stat->owner_mask)
	visible = CBIT_TEST(*dump_stat->owner_mask, id);
    if (visible)
	storeAppendPrintf(sentry, "%2d\t %-20s\t %5d\t %6.2f\n",
	    id, name, count, xdiv(count, dump_stat->busyDestroyedCount));
}

static void
httpHeaderFldsPerHdrDumper(StoreEntry * sentry, int idx, double val, double size, int count)
{
    if (count)
	storeAppendPrintf(sentry, "%2d\t %5d\t %5d\t %6.2f\n",
	    idx, (int) val, count,
	    xpercent(count, dump_stat->destroyedCount));
}


static void
httpHeaderStatDump(const HttpHeaderStat * hs, StoreEntry * e)
{
    assert(hs && e);

    dump_stat = hs;
    storeAppendPrintf(e, "\nHeader Stats: %s\n", hs->label);
    storeAppendPrintf(e, "\nField type distribution\n");
    storeAppendPrintf(e, "%2s\t %-20s\t %5s\t %6s\n",
	"id", "name", "count", "#/header");
    statHistDump(&hs->fieldTypeDistr, e, httpHeaderFieldStatDumper);
    storeAppendPrintf(e, "\nCache-control directives distribution\n");
    storeAppendPrintf(e, "%2s\t %-20s\t %5s\t %6s\n",
	"id", "name", "count", "#/cc_field");
    statHistDump(&hs->ccTypeDistr, e, httpHdrCcStatDumper);
    storeAppendPrintf(e, "\nNumber of fields per header distribution\n");
    storeAppendPrintf(e, "%2s\t %-5s\t %5s\t %6s\n",
	"id", "#flds", "count", "%total");
    statHistDump(&hs->hdrUCountDistr, e, httpHeaderFldsPerHdrDumper);
    dump_stat = NULL;
}

void
httpHeaderStoreReport(StoreEntry * e)
{
    int i;
    http_hdr_type ht;
    assert(e);

    HttpHeaderStats[0].parsedCount =
	HttpHeaderStats[hoRequest].parsedCount + HttpHeaderStats[hoReply].parsedCount;
    HttpHeaderStats[0].ccParsedCount =
	HttpHeaderStats[hoRequest].ccParsedCount + HttpHeaderStats[hoReply].ccParsedCount;
    HttpHeaderStats[0].destroyedCount =
	HttpHeaderStats[hoRequest].destroyedCount + HttpHeaderStats[hoReply].destroyedCount;
    HttpHeaderStats[0].busyDestroyedCount =
	HttpHeaderStats[hoRequest].busyDestroyedCount + HttpHeaderStats[hoReply].busyDestroyedCount;

    for (i = 1; i < HttpHeaderStatCount; i++) {
	httpHeaderStatDump(HttpHeaderStats + i, e);
	storeAppendPrintf(e, "%s\n", "<br>");
    }
    /* field stats for all messages */
    storeAppendPrintf(e, "\nHttp Fields Stats (replies and requests)\n");
    storeAppendPrintf(e, "%2s\t %-20s\t %5s\t %6s\t %6s\n",
	"id", "name", "#alive", "%err", "%repeat");
    for (ht = 0; ht < HDR_ENUM_END; ht++) {
	HttpHeaderFieldInfo *f = Headers + ht;
	storeAppendPrintf(e, "%2d\t %-20s\t %5d\t %6.3f\t %6.3f\n",
	    f->id, strBuf(f->name), f->stat.aliveCount,
	    xpercent(f->stat.errCount, f->stat.parsCount),
	    xpercent(f->stat.repCount, f->stat.seenCount));
    }
    storeAppendPrintf(e, "Headers Parsed: %d + %d = %d\n",
	HttpHeaderStats[hoRequest].parsedCount,
	HttpHeaderStats[hoReply].parsedCount,
	HttpHeaderStats[0].parsedCount);
    storeAppendPrintf(e, "Hdr Fields Parsed: %d\n", HeaderEntryParsedCount);
}

int
httpHeaderIdByNameDef(const char *name, int name_len)
{
    if (!Headers)
	Headers = httpHeaderBuildFieldsInfo(HeadersAttrs, HDR_ENUM_END);
    return httpHeaderIdByName(name, name_len, Headers, HDR_ENUM_END);
}

const char *
httpHeaderNameById(int id)
{
    if (!Headers)
	Headers = httpHeaderBuildFieldsInfo(HeadersAttrs, HDR_ENUM_END);
    assert(id >= 0 && id < HDR_ENUM_END);
    return strBuf(Headers[id].name);
}

#ifndef	__LIBHTTP_HTTP_HEADER_RANGE_H__
#define	__LIBHTTP_HTTP_HEADER_RANGE_H__

/* iteration for HttpHdrRange */
#define HttpHdrRangeInitPos (-1)

/* local constants */
#define range_spec_unknown ((squid_off_t)-1)

/* local routines */
#define known_spec(s) ((s) != range_spec_unknown)
#define size_min(a,b) ((a) <= (b) ? (a) : (b))
#define size_diff(a,b) ((a) >= (b) ? ((a)-(b)) : 0)

/* http byte-range-spec */
struct _HttpHdrRangeSpec {
    squid_off_t offset;
    squid_off_t length;
};  
typedef struct _HttpHdrRangeSpec HttpHdrRangeSpec; 

/* There may be more than one byte range specified in the request.
 * This object holds all range specs in order of their appearence
 * in the request because we SHOULD preserve that order.
 */
struct _HttpHdrRange {
    Stack specs;
};
typedef struct _HttpHdrRange HttpHdrRange;

/* iteration for HttpHdrRange */
typedef int HttpHdrRangePos;

extern squid_off_t cfg_range_offset_limit;

extern HttpHdrRange *httpHdrRangeParseCreate(const String * range_spec);
/* returns true if ranges are valid; inits HttpHdrRange */
extern int httpHdrRangeParseInit(HttpHdrRange * range, const String * range_spec);
extern void httpHdrRangeDestroy(HttpHdrRange * range);
extern HttpHdrRange *httpHdrRangeDup(const HttpHdrRange * range);
extern HttpHdrRangeSpec *httpHdrRangeGetSpec(const HttpHdrRange * range, HttpHdrRangePos * pos);

#endif

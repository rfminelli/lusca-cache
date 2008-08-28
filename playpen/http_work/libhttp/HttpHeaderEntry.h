#ifndef	__LIBHTTP_HTTPHEADERENTRY_H__

#define	__LIBHTTP_HTTPHEADERENTRY_H__

#define assert_eid(id) assert((id) < HDR_ENUM_END)

struct _HttpHeaderEntry {
    http_hdr_type id;
    int active;
    String name;
    String value;
};
typedef struct _HttpHeaderEntry HttpHeaderEntry;

/* avoid using these low level routines */
extern HttpHeaderEntry * httpHeaderEntryCreate(http_hdr_type id, const char *name, const char *value);
extern HttpHeaderEntry * httpHeaderEntryCreate2(http_hdr_type id, String name, String value);
extern void httpHeaderEntryDestroy(HttpHeaderEntry * e);
extern HttpHeaderEntry * httpHeaderEntryClone(const HttpHeaderEntry * e);

/* new low-level routines */
extern void httpHeaderEntryInitStr(HttpHeaderEntry *e, http_hdr_type id,
    const char *name, int name_len, const char *value, int value_len);
extern void httpHeaderEntryInitString(HttpHeaderEntry *e, http_hdr_type id, String name, String value);
extern void httpHeaderEntryDone(HttpHeaderEntry *e);
extern void httpHeaderEntryCopy(HttpHeaderEntry *dst, HttpHeaderEntry *src);

#endif

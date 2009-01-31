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
extern HttpHeaderEntry * httpHeaderEntryCreateL(http_hdr_type id, const char *name, int al, const char *value, int vl);
extern HttpHeaderEntry * httpHeaderEntryCreate2(http_hdr_type id, const String *name, const String *value);
extern void httpHeaderEntryDestroy(HttpHeaderEntry * e);
extern HttpHeaderEntry * httpHeaderEntryClone(const HttpHeaderEntry * e);

#endif

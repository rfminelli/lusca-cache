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

static inline int httpHeaderEntryIsActive(HttpHeaderEntry *e) { return (e->active); };

/* avoid using these low level routines */
extern void httpHeaderEntryCreate(HttpHeaderEntry *e, http_hdr_type id, const char *name, int al, const char *value, int vl);
extern void httpHeaderEntryCreate2(HttpHeaderEntry *e, http_hdr_type id, const String *name, const String *value);

extern void httpHeaderEntryDestroy(HttpHeaderEntry * e);
extern void httpHeaderEntryClone(HttpHeaderEntry *new_e, const HttpHeaderEntry * e);

#endif

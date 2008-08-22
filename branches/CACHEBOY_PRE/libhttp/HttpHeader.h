#ifndef	__LIBHTTP_HTTPHEADER_H__

#define	__LIBHTTP_HTTPHEADER_H__

/* big mask for http headers */
typedef char HttpHeaderMask[(HDR_ENUM_END + 7) / 8];

/*iteration for headers; use HttpHeaderPos as opaque type, do not interpret */
typedef int HttpHeaderPos;

/* use this and only this to initialize HttpHeaderPos */
#define HttpHeaderInitPos (-1)

struct _HttpHeaderEntry {
    http_hdr_type id;
    int active;
    String name;
    String value;
};
typedef struct _HttpHeaderEntry HttpHeaderEntry;

struct _HttpHeader {
    /* protected, do not use these, use interface functions instead */
    Array entries;              /* parsed entries in raw format */
    HttpHeaderMask mask;        /* bit set <=> entry present */
    http_hdr_owner_type owner;  /* request or reply */
    int len;                    /* length when packed, not counting terminating '\0' */
};
typedef struct _HttpHeader HttpHeader;

extern HttpHeaderFieldInfo *Headers;

extern void httpHeaderInitLibrary(void);

/* avoid using these low level routines */
extern HttpHeaderEntry * httpHeaderEntryCreate(http_hdr_type id, const char *name, const char *value);
extern HttpHeaderEntry * httpHeaderEntryCreate2(http_hdr_type id, String name, String value);
extern void httpHeaderEntryDestroy(HttpHeaderEntry * e);
extern HttpHeaderEntry * httpHeaderEntryClone(const HttpHeaderEntry * e);
extern void httpHeaderAddClone(HttpHeader * hdr, const HttpHeaderEntry * e);
extern void httpHeaderAddEntry(HttpHeader * hdr, HttpHeaderEntry * e);
extern void httpHeaderInsertEntry(HttpHeader * hdr, HttpHeaderEntry * e, int pos);
extern HttpHeaderEntry *httpHeaderGetEntry(const HttpHeader * hdr, HttpHeaderPos * pos);
extern HttpHeaderEntry *httpHeaderFindEntry(const HttpHeader * hdr, http_hdr_type id);
extern HttpHeaderEntry *httpHeaderFindLastEntry(const HttpHeader * hdr, http_hdr_type id);

#endif

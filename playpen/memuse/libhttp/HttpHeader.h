#ifndef	__LIBHTTP_HTTPHEADER_H__
#define	__LIBHTTP_HTTPHEADER_H__

/* big mask for http headers */
typedef char HttpHeaderMask[(HDR_ENUM_END + 7) / 8];

struct _HttpHeaderEntry {
    http_hdr_type id;
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


#endif

/*
 * $Id$
 *
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

#ifndef _HTTP_HEADER_H_
#define _HTTP_HEADER_H_

struct _HttpHeaderField {
	char *name;   /* field-name  from HTTP/1.1 (no column after name!) */
	char *value;  /* field-value from HTTP/1.1 */
};

/* recognized or "known" fields */
typedef enum {
    HDR_ACCEPT,
    HDR_AGE,
    HDR_CONTENT_LENGTH,
    HDR_CONTENT_MD5,
    HDR_CONTENT_TYPE,
    HDR_DATE,
    HDR_ETAG,
    HDR_EXPIRES,
    HDR_HOST,
    HDR_IMS,
    HDR_LAST_MODIFIED,
    HDR_MAX_FORWARDS,
    HDR_PUBLIC,
    HDR_RETRY_AFTER,
    HDR_SET_COOKIE,
    HDR_UPGRADE,
    HDR_WARNING,
    HDR_PROXY_KEEPALIVE,
    HDR_MISC_END
} http_hdr_type;


struct _HttpHeader {
    /* public, read only */
    size_t packed_size;  /* packed header size (see httpHeaderPack()) */
    int field_mask;      /* bits set for present [known] fields */
    int scc_mask;        /* bits set for present server cache control directives */

    /* protected, do not use these, use interface functions instead */
    int count;           /* #headers */
    int capacity;        /* max #headers before we have to grow */
    struct _HttpHeaderField **fields;
};

typedef struct _HttpHeaderField HttpHeaderField;
typedef struct _HttpHeader HttpHeader;

/*
 * use HttpHeaderPos as opaque type, do not interpret, 
 * it is not what you think it is
 */
typedef size_t HttpHeaderPos; 

/* use this and only this to initialize HttpHeaderPos */
#define HttpHeaderInitPos (-1)

/* create/init/clean/destroy */
extern HttpHeader *httpHeaderCreate();
extern void httpHeaderInit(HttpHeader *hdr);
extern void httpHeaderClean(HttpHeader *hdr);
extern void httpHeaderDestroy(HttpHeader *hdr);

/* parse/pack/swap */
/* parse a 0-terminating buffer and fill internal structires; _end points at the first character after the header; returns true if successfull */
extern int httpHeaderParse(HttpHeader *hdr, const char *header_start, const char *header_end);
/* pack header into the buffer, does not check for overflow, check hdr.packed_size first! */
extern int httpHeaderPackInto(const HttpHeader *hdr, char *buf);
/* swap using storeAppend */
extern void httpHeaderSwap(HttpHeader *hdr, StoreEntry *entry);

/* iterate through fields with name (or find first field with name) */
extern const char *httpHeaderGetStr(const HttpHeader *hdr, const char *name, HttpHeaderPos *pos);
extern long httpHeaderGetInt(const HttpHeader *hdr, const char *name, HttpHeaderPos *pos);
extern time_t httpHeaderGetDate(const HttpHeader *hdr, const char *name, HttpHeaderPos *pos); /* rfc1123 */

/* iterate through all fields */
extern HttpHeaderField *httpHeaderGetField(const HttpHeader *hdr, const char **name, const char **value, HttpHeaderPos *pos);

/* delete field(s) by name or pos */
extern int httpHeaderDelFields(HttpHeader *hdr, const char *name);
extern void httpHeaderDelField(HttpHeader *hdr, HttpHeaderPos pos);

/* add a field (appends) */
extern const char *httpHeaderAddStr(HttpHeader *hdr, const char *name, const char *value);
extern long httpHeaderAddInt(HttpHeader *hdr, const char *name, long value);
extern time_t httpHeaderAddDate(HttpHeader *hdr, const char *name, time_t value); /* mkrfc1123 */

/* fast test if a known field is present */
extern int httpHeaderHas(const HttpHeader *hdr, http_hdr_type type);

/* get common generic-header fields */
extern size_t httpHeaderGetCacheControl(const HttpHeader *hdr);
/*
extern int httpHeaderGetVia(const HttpHeader *hdr);
*/


/* http reply-header fields */
extern int httpHeaderGetContentLength(const HttpHeader *hdr);
extern time_t httpHeaderGetExpires(const HttpHeader *hdr);
/* extern time_t httpHeaderGetLastModified(const HttpHeader *hdr); */


/* http request-header fields */
extern time_t httpHeaderGetMaxAge(const HttpHeader *hdr);  /* @?@ */
extern int httpHeaderGetMaxForwards(const HttpHeader *hdr);
extern int httpHeaderGetIMS(const HttpHeader *hdr);
extern int httpHeaderGetUserAgent(const HttpHeader *hdr);

/* http entitiy-header fields */
extern int httpHeaderGetAge(const HttpHeader *hdr);

/* put report about current header usage and other stats into a store entry */
extern void httpHeaderStoreReport(StoreEntry *e);
extern void httpHeaderStoreRepReport(StoreEntry *e);
extern void httpHeaderStoreReqReport(StoreEntry *e);



#endif /* ndef _HTTP_HEADER_H_ */

#ifndef	__LIBHTTP_HTTP_HEADER_PUT_H__
#define	__LIBHTTP_HTTP_HEADER_PUT_H__

extern void httpHeaderPutStr(HttpHeader * hdr, http_hdr_type type, const char *str);
#if STDC_HEADERS 
extern void
httpHeaderPutStrf(HttpHeader * hdr, http_hdr_type id, const char *fmt,...) PRINTF_FORMAT_ARG3;
#else
extern void httpHeaderPutStrf();
#endif
extern void httpHeaderPutInt(HttpHeader * hdr, http_hdr_type id, int number);
extern void httpHeaderPutSize(HttpHeader * hdr, http_hdr_type id, squid_off_t number);
extern void httpHeaderPutTime(HttpHeader * hdr, http_hdr_type id, time_t htime);
extern void httpHeaderInsertTime(HttpHeader * hdr, int pos, http_hdr_type id, time_t htime);
extern void httpHeaderPutAuth(HttpHeader * hdr, const char *auth_scheme, const char *realm);

#endif

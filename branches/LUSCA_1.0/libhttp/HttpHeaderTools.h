#ifndef	LIBHTTP_HTTPHEADERTOOLS_H__
#define	LIBHTTP_HTTPHEADERTOOLS_H__

extern int httpHeaderHasConnDir(const HttpHeader * hdr, const char *directive);
extern const char *getStringPrefix(const char *str, const char *end);


#endif

#ifndef	LIBHTTP_HTTPHEADERTOOLS_H__
#define	LIBHTTP_HTTPHEADERTOOLS_H__


extern HttpHeaderFieldInfo * httpHeaderBuildFieldsInfo(const HttpHeaderFieldAttrs * attrs, int count);
extern void httpHeaderDestroyFieldsInfo(HttpHeaderFieldInfo * table, int count);


#endif

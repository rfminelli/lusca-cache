#ifndef	__LIBCORE_TOOLS_H__
#define	__LIBCORE_TOOLS_H__

#define MB ((size_t)1024*1024)
extern double toMB(size_t size);
extern size_t toKB(size_t size);
extern const char *xinet_ntoa(const struct in_addr addr);

#define safe_free(x)    if (x) { xxfree(x); x = NULL; }

#define XMIN(x,y) ((x)<(y)? (x) : (y))
#define XMAX(x,y) ((x)>(y)? (x) : (y))

#endif

#ifndef	__LIBCORE_TOOLS_H__
#define	__LIBCORE_TOOLS_H__

#define MB ((size_t)1024*1024)
extern double toMB(size_t size);
extern size_t toKB(size_t size);

#define safe_free(x)    if (x) { xxfree(x); x = NULL; }

#define XMIN(x,y) ((x)<(y)? (x) : (y))
#define XMAX(x,y) ((x)>(y)? (x) : (y))
#define EBIT_SET(flag, bit)     ((void)((flag) |= ((1L<<(bit)))))
#define EBIT_CLR(flag, bit)     ((void)((flag) &= ~((1L<<(bit)))))
#define EBIT_TEST(flag, bit)    ((flag) & ((1L<<(bit))))

extern struct timeval current_time;
extern double current_dtime;
extern time_t squid_curtime;    /* 0 */

extern time_t getCurrentTime(void);

extern void libcore_fatalf(const char *fmt, ...);
typedef void FATALF_FUNC(const char *fmt, va_list args); 
extern void libcore_set_fatalf(FATALF_FUNC *f);

extern int xusleep(unsigned int usec);


#endif

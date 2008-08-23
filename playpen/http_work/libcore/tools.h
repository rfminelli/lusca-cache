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

#if LEAK_CHECK_MODE
#define LOCAL_ARRAY(type,name,size) \
        static type *local_##name=NULL; \
        type *name = local_##name ? local_##name : \
                ( local_##name = (type *)xcalloc(size, sizeof(type)) )
#else
#define LOCAL_ARRAY(type,name,size) static type name[size]
#endif

/* bit opearations on a char[] mask of unlimited length */
#define CBIT_BIT(bit)           (1<<((bit)%8))
#define CBIT_BIN(mask, bit)     (mask)[(bit)>>3]
#define CBIT_SET(mask, bit)     ((void)(CBIT_BIN(mask, bit) |= CBIT_BIT(bit)))
#define CBIT_CLR(mask, bit)     ((void)(CBIT_BIN(mask, bit) &= ~CBIT_BIT(bit)))
#define CBIT_TEST(mask, bit)    ((CBIT_BIN(mask, bit) & CBIT_BIT(bit)) != 0)

/* handy to determine the #elements in a static array */
#define countof(arr) (sizeof(arr)/sizeof(*arr))

#endif

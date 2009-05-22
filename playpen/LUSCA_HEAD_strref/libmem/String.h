#ifndef	__LIBMEM_STRING_H__
#define	__LIBMEM_STRING_H__

struct _String {
    /* never reference these directly! */
    buf_t *b;
};

typedef struct _String String;

/* Code using these define's don't treat the buffer as a NUL-terminated C string */
/* XXX note - the -uses- of these calls don't assume C-string; the String code may not yet! */

#define strLen2(s)     ( (s).b ? buf_len((s).b) : 0 )
static inline const char * strBuf2(String s) { if (s.b == NULL) return NULL; return buf_buf(s.b); }

/* this replaces String->size */
#define	strCapacity(s)	( (s).b ? buf_capacity((s).b : 0 )

#define strCat(s,str)		stringAppend(&(s), (str), strlen(str))
#define	strCatStr(ds, ss)	stringAppend(&(ds), strBuf2(ss), strLen(ss))
static inline char stringGetCh(const String *s, int pos) { return strBuf2(*s)[pos]; }

#define strCmp(s,str)		strcmp(strBuf(s), (str))
#define strNCmp(s,str,n)	strncmp(strBuf(s), (str), (n))
#define strCaseCmp(s,str)	strcasecmp(strBuf(s), (str))
#define strNCaseCmp(s,str,n)	strncasecmp(strBuf(s), (str), (n))

extern int strNCmpNull(const String *s, const char *s2, int n);
extern void stringInit(String * s, const char *str);
extern void stringLimitInit(String * s, const char *str, int len);
extern String stringDup(const String * s);
extern void stringClean(String * s);
extern void stringReset(String * s, const char *str);
extern void stringAppend(String * s, const char *buf, int len);
extern char * stringDupToC(const String *s);
extern char * stringDupToCOffset(const String *s, int offset);
extern char * stringDupSubstrToC(const String *s, int len);
extern int strChr(String *s, char c);
extern int strRChr(String *s, char c);
extern void strMakePrivate(String *s);


/*
 * These is okish, but the use case probably should be replaced with a strStr() later
 * on which maps to a zero-copy region reference.
 */
extern void strCut(String *s, int pos);

/*
 * These two functions return whether the string is set to some value, even if
 * its an empty string. A few Squid functions do if (strBuf(str)) to see if
 * something has set the string to a value; these functions replace them.
 */
#define	strIsNull(s)	( (s).b == NULL )
#define	strIsNotNull(s)	( (s).b != NULL )

/* These are legacy routines which may or may not expect NUL-termination or not */
#define strLen(s)	( strLen2(s) )
#define strBuf(s)	( strBuf2(s) )

#define strStr(s,str) ((const char*)strstr(strBuf(s), (str)))  


/* extern void stringAppendf(String *s, const char *fmt, ...) PRINTF_FORMAT_ARG2; */


extern const String StringNull; /* { NULL } */

#endif

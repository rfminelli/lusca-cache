#ifndef	__LIBMEM_STRING_H__
#define	__LIBMEM_STRING_H__

struct _String {
    /* never reference these directly! */
    unsigned short int size;    /* buffer size; 64K limit */
    unsigned short int len;     /* current length  */
    char *buf;
};

typedef struct _String String;

#define strLen(s)     ((/* const */ int)(s).len)
#define strBuf(s)     ((const char*)(s).buf)
#define strChr(s,ch)  ((const char*)strchr(strBuf(s), (ch)))
#define strRChr(s,ch) ((const char*)strrchr(strBuf(s), (ch)))
#define strStr(s,str) ((const char*)strstr(strBuf(s), (str)))  
#define strCmp(s,str)     strcmp(strBuf(s), (str))
#define strNCmp(s,str,n)     strncmp(strBuf(s), (str), (n))
#define strCaseCmp(s,str) strcasecmp(strBuf(s), (str))
#define strNCaseCmp(s,str,n) strncasecmp(strBuf(s), (str), (n))
#define strSet(s,ptr,ch) (s).buf[ptr-(s).buf] = (ch)
#define strCut(s,pos) (((s).len = pos) , ((s).buf[pos] = '\0'))
#define strCutPtr(s,ptr) (((s).len = (ptr)-(s).buf) , ((s).buf[(s).len] = '\0'))
#define strCat(s,str)  stringAppend(&(s), (str), strlen(str))
extern void stringInit(String * s, const char *str);
extern void stringLimitInit(String * s, const char *str, int len);
extern String stringDup(const String * s);
extern void stringClean(String * s);
extern void stringReset(String * s, const char *str);
extern void stringAppend(String * s, const char *buf, int len);
/* extern void stringAppendf(String *s, const char *fmt, ...) PRINTF_FORMAT_ARG2; */

extern const String StringNull; /* { 0, 0, NULL } */

#endif

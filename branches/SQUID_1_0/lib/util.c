/* $Id$ */

#include "config.h"

#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_MALLOC_H && !defined(_SQUID_FREEBSD_) && !defined(_SQUID_NEXT_)
#include <malloc.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include "util.h"

void (*failure_notify) _PARAMS((char *)) = NULL;
static char msg[128];

extern int sys_nerr;
#if NEED_SYS_ERRLIST && !defined(_SQUID_NETBSD_)
extern char *sys_errlist[];
#endif

#if XMALLOC_STATISTICS
#define DBG_MAXSIZE   (1024*1024)
#define DBG_GRAIN     (16)
#define DBG_MAXINDEX  (DBG_MAXSIZE/DBG_GRAIN)
#define DBG_INDEX(sz) (sz<DBG_MAXSIZE?(sz+DBG_GRAIN-1)/DBG_GRAIN:DBG_MAXINDEX)
static int malloc_sizes[DBG_MAXINDEX + 1];
static int dbg_stat_init = 0;

static void stat_init()
{
    int i;
    for (i = 0; i <= DBG_MAXINDEX; i++)
	malloc_sizes[i] = 0;
    dbg_stat_init = 1;
}

static int malloc_stat(sz)
     int sz;
{
    if (!dbg_stat_init)
	stat_init();
    return malloc_sizes[DBG_INDEX(sz)] += 1;
}

void malloc_statistics(func, data)
     void (*func) _PARAMS((int, int, void *));
     void *data;
{
    int i;
    for (i = 0; i <= DBG_MAXSIZE; i += DBG_GRAIN)
	func(i, malloc_sizes[DBG_INDEX(i)], data);
}
#endif /* XMALLOC_STATISTICS */



#if XMALLOC_DEBUG
#define DBG_ARRY_SZ (2<<8)
#define DBG_ARRY_BKTS (2<<8)
static void *malloc_ptrs[DBG_ARRY_BKTS][DBG_ARRY_SZ];
static int malloc_size[DBG_ARRY_BKTS][DBG_ARRY_SZ];
static int dbg_initd = 0;
static int B = 0;
static int I = 0;
static void *P;
static void *Q;

static void check_init()
{
    for (B = 0; B < DBG_ARRY_SZ; B++) {
	for (I = 0; I < DBG_ARRY_SZ; I++) {
	    malloc_ptrs[B][I] = NULL;
	    malloc_size[B][I] = 0;
	}
    }
    dbg_initd = 1;
}

static void check_free(s)
     void *s;
{
    B = (((int) s) >> 4) & 0xFF;
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if (malloc_ptrs[B][I] != s)
	    continue;
	malloc_ptrs[B][I] = NULL;
	malloc_size[B][I] = 0;
	break;
    }
    if (I == DBG_ARRY_SZ) {
	sprintf(msg, "xfree: ERROR: s=%p not found!", s);
	(*failure_notify) (msg);
    }
}

static void check_malloc(p, sz)
     void *p;
     size_t sz;
{
    B = (((int) p) >> 4) & 0xFF;
    if (!dbg_initd)
	check_init();
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if ((P = malloc_ptrs[B][I]) == NULL)
	    continue;
	Q = P + malloc_size[B][I];
	if (P <= p && p < Q) {
	    sprintf(msg, "xmalloc: ERROR: p=%p falls in P=%p+%d",
		p, P, malloc_size[B][I]);
	    (*failure_notify) (msg);
	}
    }
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if ((P = malloc_ptrs[B][I]))
	    continue;
	malloc_ptrs[B][I] = p;
	malloc_size[B][I] = (int) sz;
	break;
    }
    if (I == DBG_ARRY_SZ)
	(*failure_notify) ("xmalloc: debug out of array space!");
}
#endif

/*
 *  xmalloc() - same as malloc(3).  Used for portability.
 *  Never returns NULL; fatal on error.
 */
void *xmalloc(sz)
     size_t sz;
{
    static void *p;

    if (sz < 1)
	sz = 1;
    if ((p = malloc(sz)) == NULL) {
	if (failure_notify) {
	    sprintf(msg, "xmalloc: Unable to allocate %d bytes!\n",
		(int) sz);
	    (*failure_notify) (msg);
	} else {
	    perror("malloc");
	}
	exit(1);
    }
#if XMALLOC_DEBUG
    check_malloc(p, sz);
#endif
#if XMALLOC_STATISTICS
    malloc_stat(sz);
#endif
    return (p);
}

/*
 *  xfree() - same as free(3).  Will not call free(3) if s == NULL.
 */
void xfree(s)
     void *s;
{
#if XMALLOC_DEBUG
    check_free(s);
#endif
    if (s != NULL)
	free(s);
}

/* xxfree() - like xfree(), but we already know s != NULL */
void xxfree(s)
     void *s;
{
#if XMALLOC_DEBUG
    check_free(s);
#endif
    free(s);
}

/*
 *  xrealloc() - same as realloc(3). Used for portability.
 *  Never returns NULL; fatal on error.
 */
void *xrealloc(s, sz)
     void *s;
     size_t sz;
{
    static void *p;

    if (sz < 1)
	sz = 1;
    if ((p = realloc(s, sz)) == NULL) {
	if (failure_notify) {
	    sprintf(msg, "xrealloc: Unable to reallocate %d bytes!\n",
		(int) sz);
	    (*failure_notify) (msg);
	} else {
	    perror("realloc");
	}
	exit(1);
    }
#if XMALLOC_DEBUG
    check_malloc(p, sz);
#endif
#if XMALLOC_STATISTICS
    malloc_stat(sz);
#endif
    return (p);
}

/*
 *  xcalloc() - same as calloc(3).  Used for portability.
 *  Never returns NULL; fatal on error.
 */
void *xcalloc(n, sz)
     int n;
     size_t sz;
{
    static void *p;

    if (n < 1)
	n = 1;
    if (sz < 1)
	sz = 1;
    if ((p = calloc(n, sz)) == NULL) {
	if (failure_notify) {
	    sprintf(msg, "xcalloc: Unable to allocate %d blocks of %d bytes!\n",
		(int) n, (int) sz);
	    (*failure_notify) (msg);
	} else {
	    perror("xcalloc");
	}
	exit(1);
    }
#if XMALLOC_DEBUG
    check_malloc(p, sz * n);
#endif
#if XMALLOC_STATISTICS
    malloc_stat(sz);
#endif
    return (p);
}

/*
 *  xstrdup() - same as strdup(3).  Used for portability.
 *  Never returns NULL; fatal on error.
 */
char *xstrdup(s)
     char *s;
{
    static char *p = NULL;
    size_t sz;

    if (s == NULL) {
	if (failure_notify) {
	    (*failure_notify) ("xstrdup: tried to dup a NULL pointer!\n");
	} else {
	    fprintf(stderr, "xstrdup: tried to dup a NULL pointer!\n");
	}
	exit(1);
    }
    sz = strlen(s);
    p = xmalloc((size_t) sz + 1);
    memcpy(p, s, sz);		/* copy string */
    p[sz] = '\0';		/* terminate string */
    return (p);
}

/*
 * xstrerror() - return sys_errlist[errno];
 */
char *xstrerror()
{
    static char xstrerror_buf[BUFSIZ];

    if (errno < 0 || errno >= sys_nerr)
	return ("Unknown");
    sprintf(xstrerror_buf, "(%d) %s", errno, sys_errlist[errno]);
    return xstrerror_buf;
    /* return (sys_errlist[errno]); */
}

#if !HAVE_STRDUP
/* define for systems that don't have strdup */
char *strdup(s)
     char *s;
{
    return (xstrdup(s));
}
#endif

void xmemcpy(from, to, len)
     void *from;
     void *to;
     int len;
{
#if HAVE_MEMMOVE
    (void) memmove(from, to, len);
#elif HAVE_BCOPY
    bcopy(to, from, len);
#else
    (void) memcpy(from, to, len);
#endif
}

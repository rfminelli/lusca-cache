
/*
 * $Id$
 *
 * DEBUG: 
 * AUTHOR: Harvest Derived
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

/*
 * Copyright (c) 1994, 1995.  All rights reserved.
 *  
 *   The Harvest software was developed by the Internet Research Task
 *   Force Research Group on Resource Discovery (IRTF-RD):
 *  
 *         Mic Bowman of Transarc Corporation.
 *         Peter Danzig of the University of Southern California.
 *         Darren R. Hardy of the University of Colorado at Boulder.
 *         Udi Manber of the University of Arizona.
 *         Michael F. Schwartz of the University of Colorado at Boulder.
 *         Duane Wessels of the University of Colorado at Boulder.
 *  
 *   This copyright notice applies to software in the Harvest
 *   ``src/'' directory only.  Users should consult the individual
 *   copyright notices in the ``components/'' subdirectories for
 *   copyright information about other software bundled with the
 *   Harvest source code distribution.
 *  
 * TERMS OF USE
 *   
 *   The Harvest software may be used and re-distributed without
 *   charge, provided that the software origin and research team are
 *   cited in any use of the system.  Most commonly this is
 *   accomplished by including a link to the Harvest Home Page
 *   (http://harvest.cs.colorado.edu/) from the query page of any
 *   Broker you deploy, as well as in the query result pages.  These
 *   links are generated automatically by the standard Broker
 *   software distribution.
 *   
 *   The Harvest software is provided ``as is'', without express or
 *   implied warranty, and with no support nor obligation to assist
 *   in its use, correction, modification or enhancement.  We assume
 *   no liability with respect to the infringement of copyrights,
 *   trade secrets, or any patents, and are not responsible for
 *   consequential damages.  Proper use of the Harvest software is
 *   entirely the responsibility of the user.
 *  
 * DERIVATIVE WORKS
 *  
 *   Users may make derivative works from the Harvest software, subject 
 *   to the following constraints:
 *  
 *     - You must include the above copyright notice and these 
 *       accompanying paragraphs in all forms of derivative works, 
 *       and any documentation and other materials related to such 
 *       distribution and use acknowledge that the software was 
 *       developed at the above institutions.
 *  
 *     - You must notify IRTF-RD regarding your distribution of 
 *       the derivative work.
 *  
 *     - You must clearly notify users that your are distributing 
 *       a modified version and not the original Harvest software.
 *  
 *     - Any derivative product is also subject to these copyright 
 *       and use restrictions.
 *  
 *   Note that the Harvest software is NOT in the public domain.  We
 *   retain copyright, as specified above.
 *  
 * HISTORY OF FREE SOFTWARE STATUS
 *  
 *   Originally we required sites to license the software in cases
 *   where they were going to build commercial products/services
 *   around Harvest.  In June 1995 we changed this policy.  We now
 *   allow people to use the core Harvest software (the code found in
 *   the Harvest ``src/'' directory) for free.  We made this change
 *   in the interest of encouraging the widest possible deployment of
 *   the technology.  The Harvest software is really a reference
 *   implementation of a set of protocols and formats, some of which
 *   we intend to standardize.  We encourage commercial
 *   re-implementations of code complying to this set of standards.  
 */

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
#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_GNUMALLLOC_H
#include <gnumalloc.h>
#elif HAVE_MALLOC_H && !defined(_SQUID_FREEBSD_) && !defined(_SQUID_NEXT_)
#include <malloc.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include "util.h"
#include "snprintf.h"

void (*failure_notify) (const char *) = NULL;
static char msg[128];

extern int sys_nerr;

#if XMALLOC_STATISTICS
#define DBG_MAXSIZE   (1024*1024)
#define DBG_GRAIN     (16)
#define DBG_MAXINDEX  (DBG_MAXSIZE/DBG_GRAIN)
#define DBG_INDEX(sz) (sz<DBG_MAXSIZE?(sz+DBG_GRAIN-1)/DBG_GRAIN:DBG_MAXINDEX)
static int malloc_sizes[DBG_MAXINDEX + 1];
static int dbg_stat_init = 0;

static void
stat_init(void)
{
    int i;
    for (i = 0; i <= DBG_MAXINDEX; i++)
	malloc_sizes[i] = 0;
    dbg_stat_init = 1;
}

static int
malloc_stat(int sz)
{
    if (!dbg_stat_init)
	stat_init();
    return malloc_sizes[DBG_INDEX(sz)] += 1;
}

void
malloc_statistics(void (*func) (int, int, void *), void *data)
{
    int i;
    for (i = 0; i <= DBG_MAXSIZE; i += DBG_GRAIN)
	func(i, malloc_sizes[DBG_INDEX(i)], data);
}
#endif /* XMALLOC_STATISTICS */



#if XMALLOC_TRACE
char *xmalloc_file = "";
int xmalloc_line = 0;
char *xmalloc_func = "";
static int xmalloc_count = 0;
int xmalloc_trace = 0;		/* Enable with -m option */
size_t xmalloc_total = 0;
#undef xmalloc
#undef xfree
#undef xxfree
#undef xrealloc
#undef xcalloc
#undef xstrdup
#endif

#if XMALLOC_DEBUG
#define DBG_ARRY_SZ (1<<10)
#define DBG_ARRY_BKTS (1<<8)
static void *(*malloc_ptrs)[DBG_ARRY_SZ];
static int malloc_size[DBG_ARRY_BKTS][DBG_ARRY_SZ];
#if XMALLOC_TRACE
static char *malloc_file[DBG_ARRY_BKTS][DBG_ARRY_SZ];
static short malloc_line[DBG_ARRY_BKTS][DBG_ARRY_SZ];
static int malloc_count[DBG_ARRY_BKTS][DBG_ARRY_SZ];
#endif
static int dbg_initd = 0;

static void
check_init(void)
{
    int B = 0, I = 0;
    /* calloc the ptrs so that we don't see them when hunting lost memory */
    malloc_ptrs = calloc(DBG_ARRY_BKTS, sizeof(*malloc_ptrs));
    for (B = 0; B < DBG_ARRY_BKTS; B++) {
	for (I = 0; I < DBG_ARRY_SZ; I++) {
	    malloc_ptrs[B][I] = NULL;
	    malloc_size[B][I] = 0;
#if XMALLOC_TRACE
	    malloc_file[B][I] = NULL;
	    malloc_line[B][I] = 0;
	    malloc_count[B][I] = 0;
#endif
	}
    }
    dbg_initd = 1;
}

static void
check_free(void *s)
{
    int B, I;
    B = (((int) s) >> 4) & 0xFF;
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if (malloc_ptrs[B][I] != s)
	    continue;
	malloc_ptrs[B][I] = NULL;
	malloc_size[B][I] = 0;
#if XMALLOC_TRACE
	malloc_file[B][I] = NULL;
	malloc_line[B][I] = 0;
	malloc_count[B][I] = 0;
#endif
	break;
    }
    if (I == DBG_ARRY_SZ) {
	snprintf(msg, 128, "xfree: ERROR: s=%p not found!", s);
	(*failure_notify) (msg);
    }
}

static void
check_malloc(void *p, size_t sz)
{
    void *P, *Q;
    int B, I;
    if (!dbg_initd)
	check_init();
    B = (((int) p) >> 4) & 0xFF;
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if (!(P = malloc_ptrs[B][I]))
	    continue;
	Q = P + malloc_size[B][I];
	if (P <= p && p < Q) {
	    snprintf(msg, 128, "xmalloc: ERROR: p=%p falls in P=%p+%d",
		p, P, malloc_size[B][I]);
	    (*failure_notify) (msg);
	}
    }
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if (malloc_ptrs[B][I])
	    continue;
	malloc_ptrs[B][I] = p;
	malloc_size[B][I] = (int) sz;
#if XMALLOC_TRACE
	malloc_file[B][I] = xmalloc_file;
	malloc_line[B][I] = xmalloc_line;
	malloc_count[B][I] = xmalloc_count;
#endif
	break;
    }
    if (I == DBG_ARRY_SZ)
	(*failure_notify) ("xmalloc: debug out of array space!");
}
#endif

#if XMALLOC_TRACE && !HAVE_MALLOCBLKSIZE
int
mallocblksize(void *p)
{
    int B, I;
    B = (((int) p) >> 4) & 0xFF;
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if (malloc_ptrs[B][I] == p)
	    return malloc_size[B][I];
    }
    return 0;
}
#endif

#ifdef XMALLOC_TRACE
static char *
malloc_file_name(void *p)
{
    int B, I;
    B = (((int) p) >> 4) & 0xFF;
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if (malloc_ptrs[B][I] == p)
	    return malloc_file[B][I];
    }
    return 0;
}
int
malloc_line_number(void *p)
{
    int B, I;
    B = (((int) p) >> 4) & 0xFF;
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if (malloc_ptrs[B][I] == p)
	    return malloc_line[B][I];
    }
    return 0;
}
int
malloc_number(void *p)
{
    int B, I;
    B = (((int) p) >> 4) & 0xFF;
    for (I = 0; I < DBG_ARRY_SZ; I++) {
	if (malloc_ptrs[B][I] == p)
	    return malloc_count[B][I];
    }
    return 0;
}
static void
xmalloc_show_trace(void *p, int sign)
{
    int statMemoryAccounted();
    static size_t last_total = 0, last_accounted = 0, last_mallinfo = 0;
    struct mallinfo mp = mallinfo();
    size_t accounted = statMemoryAccounted();
    size_t mi = mp.uordblks + mp.usmblks + mp.hblkhd;
    size_t sz;
    sz = mallocblksize(p) * sign;
    xmalloc_total += sz;
    xmalloc_count += sign > 0;
    if (xmalloc_trace) {
	fprintf(stderr, "%c%8p size=%5d/%d acc=%5d/%d mallinfo=%5d/%d %s:%d %s",
	    sign > 0 ? '+' : '-', p,
	    (int) xmalloc_total - last_total, (int) xmalloc_total,
	    (int) accounted - last_accounted, (int) accounted,
	    (int) mi - last_mallinfo, (int) mi,
	    xmalloc_file, xmalloc_line, xmalloc_func);
	if (sign < 0)
	    fprintf(stderr, " (%d %s:%d)\n", malloc_number(p), malloc_file_name(p), malloc_line_number(p));
	else
	    fprintf(stderr, " %d\n", xmalloc_count);
    }
    last_total = xmalloc_total;
    last_accounted = accounted;
    last_mallinfo = mi;
}
short (*malloc_refs)[DBG_ARRY_SZ];
char **xmalloc_leak_test;
#define XMALLOC_LEAK_CHECKED (1<<15)
#define XMALLOC_LEAK_ALIGN (4)
static int
xmalloc_scan_region(void *start, int size)
{
    int B, I;
    char *ptr = start;
    char *end = ptr + size - XMALLOC_LEAK_ALIGN;
    int found = 0;
    while (ptr <= end) {
	void *p = *(void **) ptr;
	if (p && p != start) {
	    B = (((int) p) >> 4) & 0xFF;
	    for (I = 0; I < DBG_ARRY_SZ; I++) {
		if (malloc_ptrs[B][I] == p) {
		    if (!malloc_refs[B][I])
			found++;
		    malloc_refs[B][I]++;
		}
	    }
	}
	ptr += XMALLOC_LEAK_ALIGN;
    }
    return found;
}
extern void _etext;
void
xmalloc_find_leaks(void)
{
    int B, I;
    int found;
    int leak_sum = 0;
    fprintf(stderr, "Searching for memory references...\n");
    malloc_refs = xcalloc(DBG_ARRY_BKTS, sizeof(*malloc_refs));
    found = xmalloc_scan_region(&_etext, (void *) sbrk(0) - (void *) &_etext);
    while (found) {
	found = 0;
	for (I = 0; I < DBG_ARRY_SZ && !found; I++) {
	    for (B = 0; B < DBG_ARRY_BKTS; B++) {
		if (malloc_refs[B][I] > 0) {
		    malloc_refs[B][I] |= XMALLOC_LEAK_CHECKED;
		    found += xmalloc_scan_region(malloc_ptrs[B][I],
			malloc_size[B][I]);
		}
	    }
	}
    }
    for (B = 0; B < DBG_ARRY_BKTS; B++) {
	for (I = 0; I < DBG_ARRY_SZ; I++) {
	    if (malloc_ptrs[B][I] && malloc_refs[B][I] == 0) {
		/* Found a leak... */
		fprintf(stderr, "Leak found: %p", malloc_ptrs[B][I]);
		fprintf(stderr, " %s", malloc_file[B][I]);
		fprintf(stderr, ":%d", malloc_line[B][I]);
		fprintf(stderr, " size %d\n", malloc_size[B][I]);
		fprintf(stderr, " allocation %d", malloc_count[B][I]);
		leak_sum += malloc_size[B][I];
	    }
	}
    }
    if (leak_sum) {
	fprintf(stderr, "Total leaked memory: %d\n", leak_sum);
    } else {
	fprintf(stderr, "No memory leaks detected\n");
    }
}
void
xmalloc_dump_map(void)
{
    int B, I;
    fprintf(stderr, "----- Memory map ----\n");
    for (B = 0; B < DBG_ARRY_BKTS; B++) {
	for (I = 0; I < DBG_ARRY_SZ; I++) {
	    if (malloc_ptrs[B][I]) {
		printf("%p %s:%d size %d allocation %d references %d\n",
		    malloc_ptrs[B][I], malloc_file[B][I], malloc_line[B][I],
		    malloc_size[B][I], malloc_count[B][I],
		    malloc_refs[B][I] & (XMALLOC_LEAK_CHECKED - 1));
	    }
	}
    }
    fprintf(stderr, "----------------------\n");
}
#endif /* XMALLOC_TRACE */

/*
 *  xmalloc() - same as malloc(3).  Used for portability.
 *  Never returns NULL; fatal on error.
 */
void *
xmalloc(size_t sz)
{
    static void *p;

    if (sz < 1)
	sz = 1;
    if ((p = malloc(sz)) == NULL) {
	if (failure_notify) {
	    snprintf(msg, 128, "xmalloc: Unable to allocate %d bytes!\n",
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
#if XMALLOC_TRACE
    xmalloc_show_trace(p, 1);
#endif
    return (p);
}

/*
 *  xfree() - same as free(3).  Will not call free(3) if s == NULL.
 */
void
xfree(void *s)
{
#if XMALLOC_TRACE
    xmalloc_show_trace(s, -1);
#endif
#if XMALLOC_DEBUG
    check_free(s);
#endif
    if (s != NULL)
	free(s);
}

/* xxfree() - like xfree(), but we already know s != NULL */
void
xxfree(void *s)
{
#if XMALLOC_TRACE
    xmalloc_show_trace(s, -1);
#endif
#if XMALLOC_DEBUG
    check_free(s);
#endif
    free(s);
}

/*
 *  xrealloc() - same as realloc(3). Used for portability.
 *  Never returns NULL; fatal on error.
 */
void *
xrealloc(void *s, size_t sz)
{
    static void *p;

#if XMALLOC_TRACE
    xmalloc_show_trace(s, -1);
#endif

    if (sz < 1)
	sz = 1;
    if ((p = realloc(s, sz)) == NULL) {
	if (failure_notify) {
	    snprintf(msg, 128, "xrealloc: Unable to reallocate %d bytes!\n",
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
#if XMALLOC_TRACE
    xmalloc_show_trace(p, 1);
#endif
    return (p);
}

/*
 *  xcalloc() - same as calloc(3).  Used for portability.
 *  Never returns NULL; fatal on error.
 */
void *
xcalloc(int n, size_t sz)
{
    static void *p;

    if (n < 1)
	n = 1;
    if (sz < 1)
	sz = 1;
    if ((p = calloc(n, sz)) == NULL) {
	if (failure_notify) {
	    snprintf(msg, 128, "xcalloc: Unable to allocate %d blocks of %d bytes!\n",
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
#if XMALLOC_TRACE
    xmalloc_show_trace(p, 1);
#endif
    return (p);
}

/*
 *  xstrdup() - same as strdup(3).  Used for portability.
 *  Never returns NULL; fatal on error.
 */
char *
xstrdup(const char *s)
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
    sz = strlen(s)+1;
    p = xmalloc(sz);
    memcpy(p, s, sz);		/* copy string, including terminating character */
    return p;
}

/*
 * xstrerror() - strerror() wrapper
 */
const char *
xstrerror(void)
{
    static char xstrerror_buf[BUFSIZ];
    if (errno < 0 || errno >= sys_nerr)
	return ("Unknown");
    snprintf(xstrerror_buf, BUFSIZ, "(%d) %s", errno, strerror(errno));
    return xstrerror_buf;
}

#if NOT_NEEDED
/*
 * xbstrerror with argument for late notification */

const char *
xbstrerror(int err)
{
    static char xbstrerror_buf[BUFSIZ];
    if (err < 0 || err >= sys_nerr)
	return ("Unknown");
    snprintf(xbstrerror_buf, BUFSIZ, "(%d) %s", err, strerror(err));
    return xbstrerror_buf;
}
#endif

void
Tolower(char *q)
{
    char *s = q;
    while (*s) {
	*s = tolower((unsigned char) *s);
	s++;
    }
}

int
tvSubMsec(struct timeval t1, struct timeval t2)
{
    return (t2.tv_sec - t1.tv_sec) * 1000 +
	(t2.tv_usec - t1.tv_usec) / 1000;
}

int
tvSubUsec(struct timeval t1, struct timeval t2)
{
    return (t2.tv_sec - t1.tv_sec) * 1000000 +
	(t2.tv_usec - t1.tv_usec);
}

double
tvSubDsec(struct timeval t1, struct timeval t2)
{
    return (double) (t2.tv_sec - t1.tv_sec) +
	(double) (t2.tv_usec - t1.tv_usec) / 1000000.0;
}

/*
 *  xstrncpy() - similar to strncpy(3) but terminates string
 *  always with '\0' if (n != 0 and dst != NULL), 
 *  and doesn't do padding
 */
char *
xstrncpy(char *dst, const char *src, size_t n)
{
    if (!n || !dst)
	return dst;
    if (src)
	while (--n != 0 && *src != '\0')
	    *dst++ = *src++;
    *dst = '\0';
    return dst;
}

/* returns the number of leading white spaces in str; handy in skipping ws */
size_t
xcountws(const char *str)
{
    size_t count = 0;
    if (str) {
	while (isspace(*str)) {
	    str++;
	    count++;
	}
    }
    return count;
}


/*
 * $Id: String.c 10771 2006-05-20 21:39:39Z hno $
 *
 * DEBUG: section 67    String
 * AUTHOR: Duane Wessels
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

#include "../include/config.h"
#include "../include/util.h"
#include "../include/Stack.h"
#include "../libcore/valgrind.h"
#include "../libcore/gb.h"
#include "../libcore/varargs.h" /* required for tools.h */
#include "../libcore/tools.h"
#include "../libcore/dlink.h"
#include "../libcore/debug.h"

#include "MemPool.h"
#include "MemStr.h"
#include "buf.h"
#include "String.h"

const String StringNull = { NULL };

/*
 * Call this function before writing to a String. This function creates
 * a local buf if required.
 */
void
strMakePrivate(String *s)
{
	buf_t *b;

	if (! s->b)
		return;

	if (buf_refcnt(s->b) == 1)		/* already private */
		return;

	/* XXX are there off-by-one errors due to the NUL termination and the size logic in buf_ routines? */
	b = buf_create_size(buf_capacity(s->b));
	assert(b);
	buf_append(b, buf_buf(s->b), buf_len(s->b), BF_APPEND_NUL);
	s->b = buf_deref(s->b);
	s->b = b;	/* is now a private copy */
}

void
stringInit(String * s, const char *str)
{
    assert(s);
    if (str)
	stringLimitInit(s, str, strlen(str));
    else
	*s = StringNull;
}

void
stringLimitInit(String * s, const char *str, int len)
{
    assert(s && str);
    s->b = buf_create_size(len + 1);		/* Trailing \0 */
    buf_append(s->b, str, len, BF_APPEND_NUL);
}

String
stringDup(const String * s)
{
    String dup;
    assert(s);
    dup.b = buf_ref(s->b);
    return dup;
}

void
stringClean(String * s)
{
    assert(s);
    if (s->b)
        (void) buf_deref(s->b);
    *s = StringNull;
}

void
stringReset(String * s, const char *str)
{
    stringClean(s);
    stringInit(s, str);
}

void
stringAppend(String * s, const char *str, int len)
{
    assert(s);
    assert(str && len >= 0);
    strMakePrivate(s);
    if (s->b == NULL)
        s->b = buf_create_size(len + 1);
    (void) buf_append(s->b, str, len, BF_APPEND_NUL);
}

/*
 * This routine REQUIRES the string to be something and not NULL
 */
char *
stringDupToCOffset(String *s, int offset)
{
	char *d;
	assert(s->b);
	assert(offset <= strLen(*s));
	d = xmalloc(strLen(*s) + 1 - offset);
	memcpy(d, strBuf(*s),  strLen(*s) - offset);
	d[strLen(*s) - offset] = '\0';
	return d;
}

char *
stringDupToC(String *s)
{
	return stringDupToCOffset(s, 0);
}

char *
stringDupSubstrToC(String *s, int len)
{
	char *d;
	int l = XMIN(len, strLen(*s));
	assert(s->b);
	assert(len <= strLen(*s));
	d = xmalloc(l + 1);
	memcpy(d, strBuf(*s), l + 1);
	d[l] = '\0';
	return d;

}


/*
 * Return the offset in the string of the found character, or -1 if not
 * found.
 */
int
strChr(String *s, char ch)
{
	int i;
	for (i = 0; i < strLen(*s); i++) {
		if (strBuf(*s)[i] == ch)
			return i;
	}
	return -1;
}

int
strRChr(String *s, char ch)
{
	int i;
	for (i = strLen(*s) - 1; i <= 0; i--) {
		if (strBuf(*s)[i] == ch)
			return i;
	}
	return -1;
}

extern void
strCut(String *s, int offset)
{
	buf_truncate(s->b, offset, BF_APPEND_NUL);
}


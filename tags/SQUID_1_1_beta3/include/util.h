/*
 * $Id$
 *
 * AUTHOR: Harvest Derived
 *
 * SQUID Internet Object Cache  http://www.nlanr.net/Squid/
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
#ifndef _UTIL_H_
#define _UTIL_H_

#include "config.h"
#include <stdio.h>
#include <time.h>

#if !defined(SQUIDHOSTNAMELEN)
#include <sys/param.h>
#ifndef _SQUID_NETDB_H_		/* need protection on NEXTSTEP */
#define _SQUID_NETDB_H_
#include <netdb.h>
#endif
#if !defined(MAXHOSTNAMELEN) || (MAXHOSTNAMELEN < 128)
#define SQUIDHOSTNAMELEN 128
#else
#define SQUIDHOSTNAMELEN MAXHOSTNAMELEN
#endif
#endif

#if !HAVE_STRDUP
extern char *strdup __P((char *));
#endif
extern char *xstrdup __P((char *));	/* Duplicate a string */

/* from xmalloc.c */
void *xmalloc __P((size_t));	/* Wrapper for malloc(3) */
void *xrealloc __P((void *, size_t));	/* Wrapper for realloc(3) */
void *xcalloc __P((int, size_t));	/* Wrapper for calloc(3) */
void xfree __P((void *));	/* Wrapper for free(3) */
void xxfree __P((void *));	/* Wrapper for free(3) */
char *xstrdup __P((char *));
char *xstrerror __P((void));
char *getfullhostname __P((void));
void xmemcpy __P((void *, void *, int));

#if XMALLOC_STATISTICS
void malloc_statistics __P((void (*)__P((int, int, void *)), void *));
#endif

/* from debug.c */
#ifndef MAX_DEBUG_LEVELS
#define MAX_DEBUG_LEVELS 256
#endif /* MAX_DEBUG_LEVELS */

#ifndef MAIN
extern int Harvest_do_debug;
extern int Harvest_debug_levels[];
#endif /* MAIN */

#undef debug_ok_fast
#if USE_NO_DEBUGGING
#define debug_ok_fast(S,L) 0
#else
#define debug_ok_fast(S,L) \
        ( \
        (Harvest_do_debug) && \
        ((Harvest_debug_levels[S] == -2) || \
         ((Harvest_debug_levels[S] != -1) && \
           ((L) <= Harvest_debug_levels[S]))) \
        )
#endif /* USE_NO_DEBUGGING */

#undef Debug
#if USE_NO_DEBUGGING
#define Debug(section, level, X) /* empty */;
#else
#define Debug(section, level, X) \
        {if (debug_ok_fast((section),(level))) {Log X;}}
#endif

void debug_flag __P((char *));

char *mkhttpdlogtime __P((time_t *));
extern char *mkrfc850 __P((time_t));
extern time_t parse_rfc850 __P((char *str));
extern void init_log3 __P((char *pn, FILE * a, FILE * b));
extern void debug_init __P((void));
extern void log_errno2 __P((char *, int, char *));

#if __STDC__
extern void Log __P((char *,...));
extern void errorlog __P((char *,...));
#else
extern void Log __P(());
extern void errorlog __P(());
#endif /* __STDC__ */

extern void Tolower __P((char *));

extern char *uudecode __P((char *));

#endif /* ndef _UTIL_H_ */

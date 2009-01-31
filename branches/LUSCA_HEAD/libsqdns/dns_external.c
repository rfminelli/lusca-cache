
/*
 * $Id$
 *
 * DEBUG: section 34    Dnsserver interface
 * AUTHOR: Harvest Derived
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

#include "../include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
  
#include "../include/Array.h"
#include "../include/Stack.h"
#include "../include/util.h"
#include "../include/hash.h"
#include "../include/rfc1035.h"
#include "../libcore/valgrind.h"
#include "../libcore/varargs.h"
#include "../libcore/debug.h"
#include "../libcore/kb.h"
#include "../libcore/gb.h"
#include "../libcore/tools.h"
#include "../libcore/dlink.h"
 
#include "../libmem/MemPool.h"
#include "../libmem/MemBufs.h"
#include "../libmem/MemBuf.h"
#include "../libmem/wordlist.h"
 
#include "../libcb/cbdata.h"
 
#include "../libiapp/iapp_ssl.h"
#include "../libiapp/comm.h"
#include "../libiapp/event.h"
#include "../libiapp/pconn_hist.h"

#include "../libhelper/ipc.h"
#include "../libhelper/helper.h"
 
#if defined(_SQUID_CYGWIN_)
#include <sys/ioctl.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#include "dns.h"
#include "dns_external.h"


/* MS VisualStudio Projects are monolithic, so we need the following
 * #if to exclude the external DNS code from compile process when
 * using Internal DNS.
 */
#if USE_DNSSERVERS
helper *dnsservers = NULL;

void
dnsInit(const char *dnsProg, int dnsChildren, wordlist *dnsNs, int res_defnames)
{
    wordlist *w;
    if (!dnsProg)
	return;
    if (dnsservers == NULL)
	dnsservers = helperCreate("dnsserver");
    dnsservers->n_to_start = dnsChildren;
    dnsservers->ipc_type = IPC_STREAM;
    assert(dnsservers->cmdline == NULL);
    wordlistAdd(&dnsservers->cmdline, dnsProg);
    if (res_defnames)
	wordlistAdd(&dnsservers->cmdline, "-D");
    for (w = dnsNs; w != NULL; w = w->next) {
	wordlistAdd(&dnsservers->cmdline, "-s");
	wordlistAdd(&dnsservers->cmdline, w->key);
    }
    helperOpenServers(dnsservers);
}

void
dnsShutdown(void)
{
    if (!dnsservers)
	return;
    helperShutdown(dnsservers);
    wordlistDestroy(&dnsservers->cmdline);
    if (!shutting_down)
	return;
    helperFree(dnsservers);
    dnsservers = NULL;
}

void
dnsSubmit(const char *lookup, HLPCB * callback, void *data)
{
    char buf[256];
    static time_t first_warn = 0;
    snprintf(buf, 256, "%s\n", lookup);
    if (dnsservers->stats.queue_size >= dnsservers->n_running * 2) {
	if (first_warn == 0)
	    first_warn = squid_curtime;
	if (squid_curtime - first_warn > 3 * 60)
	    libcore_fatalf("DNS servers not responding for 3 minutes: %d", first_warn);
	debug(34, 1) ("dnsSubmit: queue overload, rejecting %s\n", lookup);
	callback(data, (char *) "$fail Temporary network problem, please retry later");
	return;
    }
    first_warn = 0;
    helperSubmit(dnsservers, buf, callback, data);
}

#endif /* USE_DNSSERVERS */

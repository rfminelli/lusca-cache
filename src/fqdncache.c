
/*
 * $Id$
 *
 * DEBUG: section 35    FQDN Cache
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

#include "squid.h"

#define MAX_LINELEN (4096)

#define MAX_FQDN		 1024	/* Maximum cached FQDN */
#define FQDN_LOW_WATER       90
#define FQDN_HIGH_WATER      95

struct _fqdn_pending {
    FQDNH *handler;
    void *handlerData;
    struct _fqdn_pending *next;
};

struct fqdncacheQueueData {
    struct fqdncacheQueueData *next;
    fqdncache_entry *f;
};

static struct {
    int requests;
    int replies;
    int hits;
    int misses;
    int pending_hits;
    int negative_hits;
    int errors;
    int avg_svc_time;
    int ghba_calls;		/* # calls to blocking gethostbyaddr() */
} FqdncacheStats;

static dlink_list lru_list;

static void fqdncache_dnsHandleRead(int, void *);
static fqdncache_entry *fqdncache_parsebuffer(const char *buf, dnsserver_t *);
static void fqdncache_purgelru(void);
static void fqdncache_release(fqdncache_entry *);
static fqdncache_entry *fqdncache_create(const char *name);
static void fqdncache_call_pending(fqdncache_entry *);
static void fqdncacheAddHostent(fqdncache_entry *, const struct hostent *);
static int fqdncacheHasPending(const fqdncache_entry *);
static fqdncache_entry *fqdncache_get(const char *);
static FQDNH dummy_handler;
static int fqdncacheExpiredEntry(const fqdncache_entry *);
static void fqdncacheAddPending(fqdncache_entry *, FQDNH *, void *);
static void fqdncacheEnqueue(fqdncache_entry *);
static void *fqdncacheDequeue(void);
static void fqdncache_dnsDispatch(dnsserver_t *, fqdncache_entry *);
static void fqdncacheChangeKey(fqdncache_entry * i);
static void fqdncacheLockEntry(fqdncache_entry * f);
static void fqdncacheUnlockEntry(fqdncache_entry * f);

static hash_table *fqdn_table = NULL;
static struct fqdncacheQueueData *fqdncacheQueueHead = NULL;
static struct fqdncacheQueueData **fqdncacheQueueTailP = &fqdncacheQueueHead;

static char fqdncache_status_char[] =
{
    'C',
    'N',
    'P',
    'D'
};

static long fqdncache_low = 180;
static long fqdncache_high = 200;

static void
fqdncacheEnqueue(fqdncache_entry * f)
{
    struct fqdncacheQueueData *new = xcalloc(1, sizeof(struct fqdncacheQueueData));
    new->f = f;
    *fqdncacheQueueTailP = new;
    fqdncacheQueueTailP = &new->next;
}

static void *
fqdncacheDequeue(void)
{
    struct fqdncacheQueueData *old = NULL;
    fqdncache_entry *f = NULL;
    if (fqdncacheQueueHead) {
	f = fqdncacheQueueHead->f;
	old = fqdncacheQueueHead;
	fqdncacheQueueHead = fqdncacheQueueHead->next;
	if (fqdncacheQueueHead == NULL)
	    fqdncacheQueueTailP = &fqdncacheQueueHead;
	safe_free(old);
    }
    if (f && f->status != FQDN_PENDING)
	debug_trap("fqdncacheDequeue: status != FQDN_PENDING");
    return f;
}

/* removes the given fqdncache entry */
static void
fqdncache_release(fqdncache_entry * f)
{
    int k;
    assert(f->status != FQDN_PENDING);
    assert(f->status != FQDN_DISPATCHED);
    assert(f->pending_head == NULL);
    if (hash_remove_link(fqdn_table, (hash_link *) f)) {
	debug(35, 0) ("fqdncache_release: hash_remove_link() failed for '%s'\n",
	    f->name);
	return;
    }
    if (f->status == FQDN_CACHED) {
	for (k = 0; k < (int) f->name_count; k++)
	    safe_free(f->names[k]);
	debug(35, 5) ("fqdncache_release: Released FQDN record for '%s'.\n",
	    f->name);
    }
    dlinkDelete(&f->lru, &lru_list);
    safe_free(f->name);
    safe_free(f->error_message);
    safe_free(f);
    --meta_data.fqdncache_count;
}

/* return match for given name */
static fqdncache_entry *
fqdncache_get(const char *name)
{
    hash_link *e;
    static fqdncache_entry *f;

    f = NULL;
    if (fqdn_table) {
	if ((e = hash_lookup(fqdn_table, name)) != NULL)
	    f = (fqdncache_entry *) e;
    }
    return f;
}

static int
fqdncacheExpiredEntry(const fqdncache_entry * f)
{
    if (f->status == FQDN_PENDING)
	return 0;
    if (f->status == FQDN_DISPATCHED)
	return 0;
    if (f->locks != 0)
	return 0;
    if (f->expires > squid_curtime)
	return 0;
    return 1;
}

static void
fqdncache_purgelru(void)
{
    dlink_node *m;
    dlink_node *prev = NULL;
    fqdncache_entry *f;
    int removed = 0;
    for (m = lru_list.tail; m; m = prev) {
	if (meta_data.fqdncache_count < fqdncache_low)
	    break;
	prev = m->prev;
	f = m->data;
	if (f->status == FQDN_PENDING)
	    continue;
	if (f->status == FQDN_DISPATCHED)
	    continue;
	if (f->locks != 0)
	    continue;
	fqdncache_release(f);
	removed++;
    }
    debug(14, 3) ("fqdncache_purgelru: removed %d entries\n", removed);
}


/* create blank fqdncache_entry */
static fqdncache_entry *
fqdncache_create(const char *name)
{
    static fqdncache_entry *f;
    if (meta_data.fqdncache_count > fqdncache_high)
	fqdncache_purgelru();
    meta_data.fqdncache_count++;
    f = xcalloc(1, sizeof(fqdncache_entry));
    f->name = xstrdup(name);
    f->expires = squid_curtime + Config.negativeDnsTtl;
    hash_join(fqdn_table, (hash_link *) f);
    dlinkAdd(f, &f->lru, &lru_list);
    return f;
}

static void
fqdncacheAddHostent(fqdncache_entry * f, const struct hostent *hp)
{
    int k;
    f->name_count = 0;
    f->names[f->name_count++] = xstrdup((char *) hp->h_name);
    for (k = 0; hp->h_aliases[k]; k++) {
	f->names[f->name_count++] = xstrdup(hp->h_aliases[k]);
	if (f->name_count == FQDN_MAX_NAMES)
	    break;
    }
}

static fqdncache_entry *
fqdncacheAddNew(const char *name, const struct hostent *hp, fqdncache_status_t status)
{
    fqdncache_entry *f;
    assert(fqdncache_get(name) == NULL);
    debug(14, 10) ("fqdncacheAddNew: Adding '%s', status=%c\n",
	name,
	fqdncache_status_char[status]);
    f = fqdncache_create(name);
    if (hp)
	fqdncacheAddHostent(f, hp);
    f->status = status;
    f->lastref = squid_curtime;
    return f;
}

/* walks down the pending list, calling handlers */
static void
fqdncache_call_pending(fqdncache_entry * f)
{
    struct _fqdn_pending *p = NULL;
    int nhandler = 0;

    f->lastref = squid_curtime;

    fqdncacheLockEntry(f);
    while (f->pending_head != NULL) {
	p = f->pending_head;
	f->pending_head = p->next;
	if (p->handler) {
	    nhandler++;
	    dns_error_message = f->error_message;
	    p->handler((f->status == FQDN_CACHED) ? f->names[0] : NULL,
		p->handlerData);
	}
	safe_free(p);
    }
    f->pending_head = NULL;	/* nuke list */
    debug(35, 10) ("fqdncache_call_pending: Called %d handlers.\n", nhandler);
    fqdncacheUnlockEntry(f);
}

static fqdncache_entry *
fqdncache_parsebuffer(const char *inbuf, dnsserver_t * dnsData)
{
    char *buf = xstrdup(inbuf);
    char *token;
    static fqdncache_entry f;
    int k;
    int ipcount;
    int aliascount;
    debug(35, 5) ("fqdncache_parsebuffer: parsing:\n%s", inbuf);
    memset(&f, '\0', sizeof(fqdncache_entry));
    f.expires = squid_curtime + Config.positiveDnsTtl;
    f.status = FQDN_DISPATCHED;
    for (token = strtok(buf, w_space); token; token = strtok(NULL, w_space)) {
	if (!strcmp(token, "$end")) {
	    break;
	} else if (!strcmp(token, "$alive")) {
	    dnsData->answer = squid_curtime;
	} else if (!strcmp(token, "$fail")) {
	    if ((token = strtok(NULL, "\n")) == NULL)
		fatal_dump("Invalid $fail");
	    f.expires = squid_curtime + Config.negativeDnsTtl;
	    f.status = FQDN_NEGATIVE_CACHED;
	} else if (!strcmp(token, "$message")) {
	    if ((token = strtok(NULL, "\n")) == NULL)
		fatal_dump("Invalid $message");
	    f.error_message = xstrdup(token);
	} else if (!strcmp(token, "$name")) {
	    if ((token = strtok(NULL, w_space)) == NULL)
		fatal_dump("Invalid $name");
	    f.status = FQDN_CACHED;
	} else if (!strcmp(token, "$h_name")) {
	    if ((token = strtok(NULL, w_space)) == NULL)
		fatal_dump("Invalid $h_name");
	    f.names[0] = xstrdup(token);
	    f.name_count = 1;
	} else if (!strcmp(token, "$h_len")) {
	    if ((token = strtok(NULL, w_space)) == NULL)
		fatal_dump("Invalid $h_len");
	} else if (!strcmp(token, "$ipcount")) {
	    if ((token = strtok(NULL, w_space)) == NULL)
		fatal_dump("Invalid $ipcount");
	    ipcount = atoi(token);
	    for (k = 0; k < ipcount; k++) {
		if ((token = strtok(NULL, w_space)) == NULL)
		    fatal_dump("Invalid FQDN address");
	    }
	} else if (!strcmp(token, "$aliascount")) {
	    if ((token = strtok(NULL, w_space)) == NULL)
		fatal_dump("Invalid $aliascount");
	    aliascount = atoi(token);
	    for (k = 0; k < aliascount; k++) {
		if ((token = strtok(NULL, w_space)) == NULL)
		    fatal_dump("Invalid alias");
	    }
	} else if (!strcmp(token, "$ttl")) {
	    if ((token = strtok(NULL, w_space)) == NULL)
		fatal_dump("Invalid $ttl");
	    f.expires = squid_curtime + atoi(token);
	} else {
	    fatal_dump("Invalid dnsserver output");
	}
    }
    xfree(buf);
    return &f;
}

static void
fqdncacheNudgeQueue(void)
{
    dnsserver_t *dnsData;
    fqdncache_entry *f = NULL;
    while ((dnsData = dnsGetFirstAvailable()) && (f = fqdncacheDequeue()))
	fqdncache_dnsDispatch(dnsData, f);
}

static void
fqdncache_dnsHandleRead(int fd, void *data)
{
    dnsserver_t *dnsData = data;
    int len;
    int n;
    fqdncache_entry *f = NULL;
    fqdncache_entry *x = NULL;

    len = read(fd,
	dnsData->ip_inbuf + dnsData->offset,
	dnsData->size - dnsData->offset);
    fd_bytes(fd, len, FD_READ);
    debug(35, 5) ("fqdncache_dnsHandleRead: Result from DNS ID %d (%d bytes)\n",
	dnsData->id, len);
    if (len <= 0) {
	if (len < 0 && ignoreErrno(errno)) {
	    commSetSelect(fd,
		COMM_SELECT_READ,
		fqdncache_dnsHandleRead,
		dnsData, 0);
	    return;
	}
	debug(35, EBIT_TEST(dnsData->flags, HELPER_CLOSING) ? 5 : 1)
	    ("FD %d: Connection from DNSSERVER #%d is closed, disabling\n",
	    fd, dnsData->id);
	dnsData->flags = 0;
	commSetSelect(fd,
	    COMM_SELECT_WRITE,
	    NULL,
	    NULL,
	    0);
	comm_close(fd);
	return;
    }
    n = ++FqdncacheStats.replies;
    DnsStats.replies++;
    dnsData->offset += len;
    dnsData->ip_inbuf[dnsData->offset] = '\0';
    f = dnsData->data;
    if (f->status != FQDN_DISPATCHED)
	fatal_dump("fqdncache_dnsHandleRead: bad status");
    if (strstr(dnsData->ip_inbuf, "$end\n")) {
	/* end of record found */
	FqdncacheStats.avg_svc_time =
	    intAverage(FqdncacheStats.avg_svc_time,
	    tvSubMsec(dnsData->dispatch_time, current_time),
	    n, FQDNCACHE_AV_FACTOR);
	if ((x = fqdncache_parsebuffer(dnsData->ip_inbuf, dnsData)) == NULL) {
	    debug(35, 0) ("fqdncache_dnsHandleRead: fqdncache_parsebuffer failed?!\n");
	} else {
	    dnsData->offset = 0;
	    dnsData->ip_inbuf[0] = '\0';
	    f->name_count = x->name_count;
	    for (n = 0; n < (int) f->name_count; n++)
		f->names[n] = x->names[n];
	    f->error_message = x->error_message;
	    f->status = x->status;
	    f->expires = x->expires;
	    fqdncache_call_pending(f);
	}
	fqdncacheUnlockEntry(f);	/* unlock from FQDN_DISPATCHED */
    } else {
	debug(14, 5) ("fqdncache_dnsHandleRead: Incomplete reply\n");
	commSetSelect(fd,
	    COMM_SELECT_READ,
	    fqdncache_dnsHandleRead,
	    dnsData,
	    0);
    }
    if (dnsData->offset == 0) {
	dnsData->data = NULL;
	EBIT_CLR(dnsData->flags, HELPER_BUSY);
	if (EBIT_TEST(dnsData->flags, HELPER_SHUTDOWN))
	    dnsShutdownServer(dnsData);
	cbdataUnlock(dnsData);
    }
    fqdncacheNudgeQueue();
}

static void
fqdncacheAddPending(fqdncache_entry * f, FQDNH * handler, void *handlerData)
{
    struct _fqdn_pending *pending = xcalloc(1, sizeof(struct _fqdn_pending));
    struct _fqdn_pending **I = NULL;
    f->lastref = squid_curtime;
    pending->handler = handler;
    pending->handlerData = handlerData;
    for (I = &(f->pending_head); *I; I = &((*I)->next));
    *I = pending;
    if (f->status == FQDN_PENDING)
	fqdncacheNudgeQueue();
}

void
fqdncache_nbgethostbyaddr(struct in_addr addr, FQDNH * handler, void *handlerData)
{
    fqdncache_entry *f = NULL;
    dnsserver_t *dnsData = NULL;
    char *name = inet_ntoa(addr);

    if (!handler)
	fatal_dump("fqdncache_nbgethostbyaddr: NULL handler");

    debug(35, 4) ("fqdncache_nbgethostbyaddr: Name '%s'.\n", name);
    FqdncacheStats.requests++;

    if (name == NULL || name[0] == '\0') {
	debug(35, 4) ("fqdncache_nbgethostbyaddr: Invalid name!\n");
	handler(NULL, handlerData);
	return;
    }
    if ((f = fqdncache_get(name))) {
	if (fqdncacheExpiredEntry(f)) {
	    fqdncache_release(f);
	    f = NULL;
	}
    }
    if (f == NULL) {
	/* MISS: No entry, create the new one */
	debug(35, 5) ("fqdncache_nbgethostbyaddr: MISS for '%s'\n", name);
	FqdncacheStats.misses++;
	f = fqdncacheAddNew(name, NULL, FQDN_PENDING);
	fqdncacheAddPending(f, handler, handlerData);
    } else if (f->status == FQDN_CACHED || f->status == FQDN_NEGATIVE_CACHED) {
	/* HIT */
	debug(35, 4) ("fqdncache_nbgethostbyaddr: HIT for '%s'\n", name);
	if (f->status == FQDN_NEGATIVE_CACHED)
	    FqdncacheStats.negative_hits++;
	else
	    FqdncacheStats.hits++;
	fqdncacheAddPending(f, handler, handlerData);
	fqdncache_call_pending(f);
	return;
    } else if (f->status == FQDN_PENDING || f->status == FQDN_DISPATCHED) {
	debug(35, 4) ("fqdncache_nbgethostbyaddr: PENDING for '%s'\n", name);
	FqdncacheStats.pending_hits++;
	fqdncacheAddPending(f, handler, handlerData);
	if (squid_curtime - f->expires > 600) {
	    debug(14, 0) ("fqdncache_nbgethostbyname: '%s' PENDING for %d seconds, aborting\n", name, squid_curtime + Config.negativeDnsTtl - f->expires);
	    fqdncacheChangeKey(f);
	    fqdncache_call_pending(f);
	}
	return;
    } else {
	fatal_dump("fqdncache_nbgethostbyaddr: BAD fqdncache_entry status");
    }

    /* for HIT, PENDING, DISPATCHED we've returned.  For MISS we continue */

    if ((dnsData = dnsGetFirstAvailable())) {
	fqdncache_dnsDispatch(dnsData, f);
    } else if (NDnsServersAlloc > 0) {
	fqdncacheEnqueue(f);
    } else {
	/* abort if we get here */
	assert(NDnsServersAlloc);
    }
}

static void
fqdncache_dnsDispatch(dnsserver_t * dns, fqdncache_entry * f)
{
    char *buf = NULL;
    assert(EBIT_TEST(dns->flags, HELPER_ALIVE));
    if (!fqdncacheHasPending(f)) {
	debug(35, 0) ("fqdncache_dnsDispatch: skipping '%s' because no handler.\n",
	    f->name);
	f->status = FQDN_NEGATIVE_CACHED;
	fqdncache_release(f);
	return;
    }
    if (f->status != FQDN_PENDING)
	debug_trap("fqdncache_dnsDispatch: status != FQDN_PENDING");
    buf = xcalloc(1, 256);
    snprintf(buf, 256, "%s\n", f->name);
    EBIT_SET(dns->flags, HELPER_BUSY);
    dns->data = f;
    f->status = FQDN_DISPATCHED;
    comm_write(dns->outpipe,
	buf,
	strlen(buf),
	NULL,			/* Handler */
	NULL,			/* Handler-data */
	xfree);
    cbdataLock(dns);
    commSetSelect(dns->outpipe,
	COMM_SELECT_READ,
	fqdncache_dnsHandleRead,
	dns,
	0);
    debug(35, 5) ("fqdncache_dnsDispatch: Request sent to DNS server #%d.\n",
	dns->id);
    dns->dispatch_time = current_time;
    DnsStats.requests++;
    DnsStats.hist[dns->id - 1]++;
    fqdncacheLockEntry(f);	/* lock while FQDN_DISPATCHED */
}


/* initialize the fqdncache */
void
fqdncache_init(void)
{
    if (fqdn_table)
	return;
    debug(35, 3) ("Initializing FQDN Cache...\n");
    memset(&FqdncacheStats, '\0', sizeof(FqdncacheStats));
    /* small hash table */
    fqdn_table = hash_create(urlcmp, 229, hash4);
    fqdncache_high = (long) (((float) MAX_FQDN *
	    (float) FQDN_HIGH_WATER) / (float) 100);
    fqdncache_low = (long) (((float) MAX_FQDN *
	    (float) FQDN_LOW_WATER) / (float) 100);
}

/* clean up the pending entries in dnsserver */
/* return 1 if we found the host, 0 otherwise */
int
fqdncacheUnregister(struct in_addr addr, void *data)
{
    char *name = inet_ntoa(addr);
    fqdncache_entry *f = NULL;
    struct _fqdn_pending *p = NULL;
    int n = 0;
    debug(35, 3) ("fqdncacheUnregister: FD %d, name '%s'\n", name);
    if ((f = fqdncache_get(name)) == NULL)
	return 0;
    if (f->status == FQDN_PENDING || f->status == FQDN_DISPATCHED) {
	for (p = f->pending_head; p; p = p->next) {
	    if (p->handlerData != data)
		continue;
	    p->handler = NULL;
	    n++;
	}
    }
    if (n == 0)
	debug_trap("fqdncacheUnregister: callback data not found");
    debug(35, 3) ("fqdncacheUnregister: unregistered %d handlers\n", n);
    return n;
}

const char *
fqdncache_gethostbyaddr(struct in_addr addr, int flags)
{
    char *name = inet_ntoa(addr);
    fqdncache_entry *f = NULL;
    struct in_addr ip;

    if (!name)
	fatal_dump("fqdncache_gethostbyaddr: NULL name");
    FqdncacheStats.requests++;
    if ((f = fqdncache_get(name))) {
	if (fqdncacheExpiredEntry(f)) {
	    fqdncache_release(f);
	    f = NULL;
	}
    }
    if (f) {
	if (f->status == FQDN_NEGATIVE_CACHED) {
	    FqdncacheStats.negative_hits++;
	    dns_error_message = f->error_message;
	    return NULL;
	} else {
	    FqdncacheStats.hits++;
	    f->lastref = squid_curtime;
	    return f->names[0];
	}
    }
    /* check if it's already a FQDN address in text form. */
    if (!safe_inet_addr(name, &ip))
	return name;
    FqdncacheStats.misses++;
    if (flags & FQDN_LOOKUP_IF_MISS)
	fqdncache_nbgethostbyaddr(addr, dummy_handler, NULL);
    return NULL;
}


/* process objects list */
void
fqdnStats(StoreEntry * sentry)
{
    fqdncache_entry *f = NULL;
    fqdncache_entry *next = NULL;
    int k;
    int ttl;
    if (fqdn_table == NULL)
	return;
    storeAppendPrintf(sentry, "{FQDN Cache Statistics:\n");
    storeAppendPrintf(sentry, "{FQDNcache Entries: %d}\n",
	meta_data.fqdncache_count);
    storeAppendPrintf(sentry, "{FQDNcache Requests: %d}\n",
	FqdncacheStats.requests);
    storeAppendPrintf(sentry, "{FQDNcache Hits: %d}\n",
	FqdncacheStats.hits);
    storeAppendPrintf(sentry, "{FQDNcache Pending Hits: %d}\n",
	FqdncacheStats.pending_hits);
    storeAppendPrintf(sentry, "{FQDNcache Negative Hits: %d}\n",
	FqdncacheStats.negative_hits);
    storeAppendPrintf(sentry, "{FQDNcache Misses: %d}\n",
	FqdncacheStats.misses);
    storeAppendPrintf(sentry, "{Blocking calls to gethostbyaddr(): %d}\n",
	FqdncacheStats.ghba_calls);
    storeAppendPrintf(sentry, "{dnsserver avg service time: %d msec}\n",
	FqdncacheStats.avg_svc_time);
    storeAppendPrintf(sentry, "}\n\n");
    storeAppendPrintf(sentry, "{FQDN Cache Contents:\n\n");

    next = (fqdncache_entry *) hash_first(fqdn_table);
    while ((f = next) != NULL) {
	next = (fqdncache_entry *) hash_next(fqdn_table);
	if (f->status == FQDN_PENDING || f->status == FQDN_DISPATCHED)
	    ttl = 0;
	else
	    ttl = (f->expires - squid_curtime);
	storeAppendPrintf(sentry, " {%-32.32s %c %6d %d",
	    f->name,
	    fqdncache_status_char[f->status],
	    ttl,
	    (int) f->name_count);
	for (k = 0; k < (int) f->name_count; k++)
	    storeAppendPrintf(sentry, " %s", f->names[k]);
	storeAppendPrintf(sentry, close_bracket);
    }
    storeAppendPrintf(sentry, close_bracket);
}

static void
dummy_handler(const char *bufnotused, void *datanotused)
{
    return;
}

static int
fqdncacheHasPending(const fqdncache_entry * f)
{
    const struct _fqdn_pending *p = NULL;
    if (f->status != FQDN_PENDING)
	return 0;
    for (p = f->pending_head; p; p = p->next)
	if (p->handler)
	    return 1;
    return 0;
}

void
fqdncacheReleaseInvalid(const char *name)
{
    fqdncache_entry *f;
    if ((f = fqdncache_get(name)) == NULL)
	return;
    if (f->status != FQDN_NEGATIVE_CACHED)
	return;
    fqdncache_release(f);
}

const char *
fqdnFromAddr(struct in_addr addr)
{
    const char *n;
    static char buf[32];
    if (Config.onoff.log_fqdn && (n = fqdncache_gethostbyaddr(addr, 0)))
	return n;
    xstrncpy(buf, inet_ntoa(addr), 32);
    return buf;
}

static void
fqdncacheLockEntry(fqdncache_entry * f)
{
    if (f->locks++ == 0) {
	dlinkDelete(&f->lru, &lru_list);
	dlinkAdd(f, &f->lru, &lru_list);
    }
}

static void
fqdncacheUnlockEntry(fqdncache_entry * f)
{
    if (f->locks == 0) {
	debug_trap("fqdncacheUnlockEntry: Entry has no locks");
	return;
    }
    f->locks--;
    if (fqdncacheExpiredEntry(f))
	fqdncache_release(f);
}

void
fqdncacheFreeMemory(void)
{
    fqdncache_entry *f;
    fqdncache_entry **list;
    int i = 0;
    int j = 0;
    int k = 0;
    list = xcalloc(meta_data.fqdncache_count, sizeof(fqdncache_entry *));
    f = (fqdncache_entry *) hash_first(fqdn_table);
    while (f && i < meta_data.fqdncache_count) {
	*(list + i) = f;
	i++;
	f = (fqdncache_entry *) hash_next(fqdn_table);
    }
    for (j = 0; j < i; j++) {
	f = *(list + j);
	for (k = 0; k < (int) f->name_count; k++)
	    safe_free(f->names[k]);
	safe_free(f->name);
	safe_free(f->error_message);
	safe_free(f);
    }
    xfree(list);
    hashFreeMemory(fqdn_table);
    fqdn_table = NULL;
}

static void
fqdncacheChangeKey(fqdncache_entry * f)
{
    static int index = 0;
    LOCAL_ARRAY(char, new_key, 256);
    hash_link *table_entry = hash_lookup(fqdn_table, f->name);
    if (table_entry == NULL) {
	debug(14, 0) ("fqdncacheChangeKey: Could not find key '%s'\n", f->name);
	return;
    }
    if (f != (fqdncache_entry *) table_entry) {
	debug_trap("fqdncacheChangeKey: f != table_entry!");
	return;
    }
    if (hash_remove_link(fqdn_table, table_entry)) {
	debug_trap("fqdncacheChangeKey: hash_remove_link() failed\n");
	return;
    }
    snprintf(new_key, 256, "%d/", ++index);
    strncat(new_key, f->name, 128);
    debug(14, 1) ("fqdncacheChangeKey: from '%s' to '%s'\n", f->name, new_key);
    safe_free(f->name);
    f->name = xstrdup(new_key);
    hash_join(fqdn_table, (hash_link *) f);
}

/* call during reconfigure phase to clear out all the
 * pending and dispatched reqeusts that got lost */
void
fqdncache_restart(void)
{
    fqdncache_entry *this;
    fqdncache_entry *next;
    if (fqdn_table == 0)
	fatal_dump("fqdncache_restart: fqdn_table == 0\n");
    while (fqdncacheDequeue());
    next = (fqdncache_entry *) hash_first(fqdn_table);
    while ((this = next) != NULL) {
	next = (fqdncache_entry *) hash_next(fqdn_table);
	if (this->status == FQDN_CACHED)
	    continue;
	if (this->status == FQDN_NEGATIVE_CACHED)
	    continue;
#if DONT
	/* else its PENDING or DISPATCHED; there are no dnsservers
	 * running, so abort it */
	this->status = FQDN_NEGATIVE_CACHED;
	fqdncache_release(this);
#endif
    }
    fqdncache_high = (long) (((float) MAX_FQDN *
	    (float) FQDN_HIGH_WATER) / (float) 100);
    fqdncache_low = (long) (((float) MAX_FQDN *
	    (float) FQDN_LOW_WATER) / (float) 100);
}

#ifdef SQUID_SNMP
u_char *
var_fqdn_entry(struct variable *vp, oid * name, int *length, int exact, int
    *var_len,
    SNMPWM ** write_method)
{
    static int current = 0;
    static long long_return;
    static char *cp = NULL;
    static fqdncache_entry *fq;
    static struct in_addr fqaddr;
    int i;
    oid newname[MAX_NAME_LEN];
    int result;
    static char snbuf[256];

    debug(49, 3) ("snmp: var_fqdn_entry called with magic=%d \n", vp->magic);
    debug(49, 3) ("snmp: var_fqdn_entry with (%d,%d)\n", *length, *var_len);
    sprint_objid(snbuf, name, *length);
    debug(49, 3) ("snmp: var_fqdn_entry oid: %s\n", snbuf);

    memcpy((char *) newname, (char *) vp->name, (int) vp->namelen * sizeof(oid));
    newname[vp->namelen] = (oid) 1;

    debug(49, 5) ("snmp var_fqdn_entry: hey, here we are.\n");

    fq = NULL;
    i = 0;
    while (fq != NULL) {
	newname[vp->namelen] = i + 1;
	result = compare(name, *length, newname, (int) vp->namelen + 1);
	if ((exact && (result == 0)) || (!exact && (result < 0))) {
	    debug(49, 5) ("snmp var_fqdn_entry: yup, a match.\n");
	    break;
	}
	i++;
	fq = NULL;
    }
    if (fq == NULL)
	return NULL;

    debug(49, 5) ("hey, matched.\n");
    memcpy((char *) name, (char *) newname, ((int) vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;
    *write_method = 0;
    *var_len = sizeof(long);	/* default length */
    sprint_objid(snbuf, newname, *length);
    debug(49, 5) ("snmp var_fqdn_entry  request for %s (%d)\n", snbuf, current);

    switch (vp->magic) {
    case NET_FQDN_ID:
	long_return = (long) i;
	return (u_char *) & long_return;
    case NET_FQDN_NAME:
	cp = fq->names[0];
	*var_len = strlen(cp);
	return (u_char *) cp;
    case NET_FQDN_IP:
	safe_inet_addr(fq->name, &fqaddr);
	long_return = (long) fqaddr.s_addr;
	return (u_char *) & long_return;
    case NET_FQDN_LASTREF:
	long_return = fq->lastref;
	return (u_char *) & long_return;
    case NET_FQDN_EXPIRES:
	long_return = fq->expires;
	return (u_char *) & long_return;
    case NET_FQDN_STATE:
	long_return = fq->status;
	return (u_char *) & long_return;
    default:
	return NULL;
    }
}
#endif

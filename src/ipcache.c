/*
 * $Id$
 *
 * DEBUG: section 14    IP Cache
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

#include "squid.h"

#define MAX_LINELEN (4096)

#define MAX_IP		 1024	/* Maximum cached IP */
#define IP_LOW_WATER       90
#define IP_HIGH_WATER      95
#define MAX_HOST_NAME	  256
#define IP_INBUF_SZ	 4096

struct _ip_pending {
    int fd;
    IPH handler;
    void *handlerData;
    struct _ip_pending *next;
};

#define DNS_FLAG_ALIVE		0x01
#define DNS_FLAG_BUSY		0x02
#define DNS_FLAG_CLOSING	0x04

typedef struct _dnsserver {
    int id;
    int flags;
    int inpipe;
    int outpipe;
    time_t lastcall;
    time_t answer;
    unsigned int offset;
    unsigned int size;
    char *ip_inbuf;
    struct timeval dispatch_time;
    ipcache_entry *ip_entry;
} dnsserver_t;

static struct {
    int requests;
    int hits;
    int misses;
    int pending_hits;
    int negative_hits;
    int dnsserver_requests;
    int dnsserver_replies;
    int errors;
    int avg_svc_time;
    int ghbn_calls;		/* # calls to blocking gethostbyname() */
    int dnsserver_hist[DefaultDnsChildrenMax];
} IpcacheStats;

typedef struct _line_entry {
    char *line;
    struct _line_entry *next;
} line_entry;

struct dnsQueueData {
    struct dnsQueueData *next;
    ipcache_entry *ip_entry;
};

static int ipcache_testname _PARAMS((void));
static dnsserver_t *dnsGetFirstAvailable _PARAMS((void));
static int ipcache_compareLastRef _PARAMS((ipcache_entry **, ipcache_entry **));
static int ipcache_reverseLastRef _PARAMS((ipcache_entry **, ipcache_entry **));
static int ipcache_create_dnsserver _PARAMS((char *command));
static int ipcache_dnsHandleRead _PARAMS((int, dnsserver_t *));
static int ipcache_parsebuffer _PARAMS((char *buf, unsigned int offset, dnsserver_t *));
static int ipcache_purgelru _PARAMS((void));
static void ipcache_release _PARAMS((ipcache_entry *));
static ipcache_entry *ipcache_GetFirst _PARAMS((void));
static ipcache_entry *ipcache_GetNext _PARAMS((void));
static ipcache_entry *ipcache_create _PARAMS((void));
static void free_lines _PARAMS((line_entry *));
static void ipcache_add_to_hash _PARAMS((ipcache_entry *));
static void ipcache_call_pending _PARAMS((ipcache_entry *));
static void ipcache_call_pending_badname _PARAMS((int fd, IPH handler, void *));
static void ipcache_add _PARAMS((char *, ipcache_entry *, struct hostent *, int));
static ipcache_entry *dnsDequeue _PARAMS(());
static void dnsEnqueue _PARAMS((ipcache_entry *));
static void dnsDispatch _PARAMS((dnsserver_t *, ipcache_entry *));
static int ipcacheHasPending _PARAMS((ipcache_entry *));
static ipcache_entry *ipcache_get _PARAMS((char *));
static int dummy_handler _PARAMS((int, struct hostent * hp, void *));
static int ipcacheExpiredEntry _PARAMS((ipcache_entry *));
static void ipcacheAddPending _PARAMS((ipcache_entry *, int fd, IPH, void *));
static struct hostent *ipcacheCheckNumeric _PARAMS((char *name));
static void ipcacheStatPrint _PARAMS((ipcache_entry *, StoreEntry *));

static dnsserver_t **dns_child_table = NULL;
static struct hostent *static_result = NULL;
static int NDnsServersAlloc = 0;
static struct dnsQueueData *dnsQueueHead = NULL;
static struct dnsQueueData **dnsQueueTailP = &dnsQueueHead;
static HashID ip_table = 0;
char *dns_error_message = NULL;	/* possible error message */
long ipcache_low = 180;
long ipcache_high = 200;

static char ipcache_status_char[] =
{
    'C',
    'N',
    'P',
    'D'
};

static int ipcache_testname()
{
    wordlist *w = NULL;
    debug(14, 1, "Performing DNS Tests...\n");
    if ((w = getDnsTestnameList()) == NULL)
	return 1;
    for (; w; w = w->next) {
	IpcacheStats.ghbn_calls++;
	if (gethostbyname(w->key) != NULL)
	    return 1;
    }
    return 0;
}

/* TCP SOCKET VERSION */
static int ipcache_create_dnsserver(command)
     char *command;
{
    int pid;
    u_short port;
    struct sockaddr_in S;
    int cfd;
    int sfd;
    int len;
    int fd;
    static char buf[128];

    cfd = comm_open(COMM_NOCLOEXEC,
	local_addr,
	0,
	"socket to dnsserver");
    if (cfd == COMM_ERROR) {
	debug(14, 0, "ipcache_create_dnsserver: Failed to create dnsserver\n");
	return -1;
    }
    len = sizeof(S);
    memset(&S, '\0', len);
    if (getsockname(cfd, (struct sockaddr *) &S, &len) < 0) {
	debug(14, 0, "ipcache_create_dnsserver: getsockname: %s\n", xstrerror());
	comm_close(cfd);
	return -1;
    }
    port = ntohs(S.sin_port);
    debug(14, 4, "ipcache_create_dnsserver: bind to local host.\n");
    listen(cfd, 1);
    getCurrentTime();
    if ((pid = fork()) < 0) {
	debug(14, 0, "ipcache_create_dnsserver: fork: %s\n", xstrerror());
	comm_close(cfd);
	return -1;
    }
    if (pid > 0) {		/* parent */
	comm_close(cfd);	/* close shared socket with child */
	/* open new socket for parent process */
	sfd = comm_open(0, local_addr, 0, NULL);	/* blocking! */
	if (sfd == COMM_ERROR)
	    return -1;
	if (comm_connect(sfd, localhost, port) == COMM_ERROR) {
	    comm_close(sfd);
	    return -1;
	}
	if (write(sfd, "$hello\n", 7) < 0) {
	    debug(14, 0, "ipcache_create_dnsserver: $hello write test failed\n");
	    comm_close(sfd);
	    return -1;
	}
	memset(buf, '\0', 128);
	if (read(sfd, buf, 128) < 0 || strcmp(buf, "$alive\n$end\n")) {
	    debug(14, 0, "ipcache_create_dnsserver: $hello read test failed\n");
	    comm_close(sfd);
	    return -1;
	}
	comm_set_fd_lifetime(sfd, -1);
	return sfd;
    }
    /* child */

    no_suid();			/* give up extra priviliges */
    dup2(cfd, 3);
    for (fd = FD_SETSIZE; fd > 3; fd--)
	close(fd);
    execlp(command, "(dnsserver)", "-t", NULL);
    debug(14, 0, "ipcache_create_dnsserver: %s: %s\n", command, xstrerror());
    _exit(1);
    return 0;
}

/* removes the given ipcache entry */
static void ipcache_release(i)
     ipcache_entry *i;
{
    ipcache_entry *result = 0;
    hash_link *table_entry = NULL;
    int k;

    if ((table_entry = hash_lookup(ip_table, i->name)) == NULL) {
	debug(14, 0, "ipcache_release: Could not find key '%s'\n", i->name);
	return;
    }
    result = (ipcache_entry *) table_entry;
    if (i != result)
	fatal_dump("ipcache_release: expected i == result!");
    if (i->status == IP_PENDING) {
	debug(14, 1, "ipcache_release: Someone called on a PENDING entry\n");
	return;
    }
    if (i->status == IP_DISPATCHED) {
	debug(14, 1, "ipcache_release: Someone called on a DISPATCHED entry\n");
	return;
    }
    if (hash_remove_link(ip_table, table_entry)) {
	debug(14, 0, "ipcache_release: hash_remove_link() failed for '%s'\n",
	    result->name);
	return;
    }
    if (result->status == IP_CACHED) {
	for (k = 0; k < (int) result->addr_count; k++)
	    safe_free(result->entry.h_addr_list[k]);
	safe_free(result->entry.h_addr_list);
	for (k = 0; k < (int) result->alias_count; k++)
	    safe_free(result->entry.h_aliases[k]);
	if (result->entry.h_aliases)
	    safe_free(result->entry.h_aliases);
	safe_free(result->entry.h_name);
	debug(14, 5, "ipcache_release: Released IP cached record for '%s'.\n",
	    result->name);
    }
    safe_free(result->name);
    safe_free(result->error_message);
    memset(result, '\0', sizeof(ipcache_entry));
    safe_free(result);
    --meta_data.ipcache_count;
    return;
}

/* return match for given name */
static ipcache_entry *ipcache_get(name)
     char *name;
{
    hash_link *e;
    static ipcache_entry *i;

    i = NULL;
    if (ip_table) {
	if ((e = hash_lookup(ip_table, name)) != NULL)
	    i = (ipcache_entry *) e;
    }
    return i;
}

/* get the first ip entry in the storage */
static ipcache_entry *ipcache_GetFirst()
{
    return (ipcache_entry *) hash_first(ip_table);
}

/* get the next ip entry in the storage for a given search pointer */
static ipcache_entry *ipcache_GetNext()
{
    return (ipcache_entry *) hash_next(ip_table);
}

static int ipcache_compareLastRef(e1, e2)
     ipcache_entry **e1, **e2;
{
    if (!e1 || !e2)
	fatal_dump(NULL);
    if ((*e1)->lastref > (*e2)->lastref)
	return (1);
    if ((*e1)->lastref < (*e2)->lastref)
	return (-1);
    return (0);
}

static int ipcache_reverseLastRef(e1, e2)
     ipcache_entry **e1, **e2;
{
    if ((*e1)->lastref < (*e2)->lastref)
	return (1);
    if ((*e1)->lastref > (*e2)->lastref)
	return (-1);
    return (0);
}

static int ipcacheExpiredEntry(i)
     ipcache_entry *i;
{
    if (i->lock)
	return 0;
    if (i->status == IP_PENDING)
	return 0;
    if (i->status == IP_DISPATCHED)
	return 0;
    if (i->ttl + i->lastref > squid_curtime)
	return 0;
    return 1;
}

/* finds the LRU and deletes */
static int ipcache_purgelru()
{
    ipcache_entry *i = NULL;
    int local_ip_count = 0;
    int local_ip_notpending_count = 0;
    int removed = 0;
    int k;
    ipcache_entry **LRU_list = NULL;
    int LRU_list_count = 0;

    LRU_list = xcalloc(meta_data.ipcache_count, sizeof(ipcache_entry *));

    for (i = ipcache_GetFirst(); i; i = ipcache_GetNext()) {
	if (ipcacheExpiredEntry(i)) {
	    ipcache_release(i);
	    removed++;
	    continue;
	}
	local_ip_count++;

	if (LRU_list_count == meta_data.ipcache_count)
	    break;
	if (i->status == IP_PENDING)
	    continue;
	if (i->status == IP_DISPATCHED)
	    continue;
	if (i->lock)
	    continue;
	local_ip_notpending_count++;
	LRU_list[LRU_list_count++] = i;
    }

    debug(14, 3, "ipcache_purgelru: ipcache_count: %5d\n", meta_data.ipcache_count);
    debug(14, 3, "                  actual count : %5d\n", local_ip_count);
    debug(14, 3, "                   high W mark : %5d\n", ipcache_high);
    debug(14, 3, "                   low  W mark : %5d\n", ipcache_low);
    debug(14, 3, "                   not pending : %5d\n", local_ip_notpending_count);
    debug(14, 3, "                LRU candidates : %5d\n", LRU_list_count);

    /* sort LRU candidate list */
    qsort((char *) LRU_list,
	LRU_list_count,
	sizeof(ipcache_entry *),
	(QS) ipcache_compareLastRef);
    for (k = 0; k < LRU_list_count; k++) {
	if (meta_data.ipcache_count < ipcache_low)
	    break;
	if (LRU_list[k] == NULL)
	    break;
	ipcache_release(LRU_list[k]);
	removed++;
    }

    debug(14, 3, "                       removed : %5d\n", removed);
    safe_free(LRU_list);
    return (removed > 0) ? 0 : -1;
}


/* create blank ipcache_entry */
static ipcache_entry *ipcache_create()
{
    static ipcache_entry *new;

    if (meta_data.ipcache_count > ipcache_high) {
	if (ipcache_purgelru() < 0)
	    debug(14, 0, "HELP!! IP Cache is overflowing!\n");
    }
    meta_data.ipcache_count++;
    new = xcalloc(1, sizeof(ipcache_entry));
    /* set default to 4, in case parser fail to get token $h_length from
     * dnsserver. */
    new->entry.h_length = 4;
    return new;

}

static void ipcache_add_to_hash(i)
     ipcache_entry *i;
{
    if (hash_join(ip_table, (hash_link *) i)) {
	debug(14, 1, "ipcache_add_to_hash: Cannot add %s (%p) to hash table %d.\n",
	    i->name, i, ip_table);
    }
    debug(14, 5, "ipcache_add_to_hash: name <%s>\n", i->name);
}


static void ipcache_add(name, i, hp, cached)
     char *name;
     ipcache_entry *i;
     struct hostent *hp;
     int cached;
{
    int addr_count;
    int alias_count;
    int k;

    if (ipcache_get(name))
	fatal_dump("ipcache_add: somebody adding a duplicate!");
    debug(14, 10, "ipcache_add: Adding name '%s' (%s).\n", name,
	cached ? "cached" : "not cached");
    i->name = xstrdup(name);
    if (cached) {
	/* count for IPs */
	addr_count = 0;
	while ((addr_count < 255) && hp->h_addr_list[addr_count])
	    ++addr_count;

	i->addr_count = addr_count;

	/* count for Alias */
	alias_count = 0;
	if (hp->h_aliases)
	    while ((alias_count < 255) && hp->h_aliases[alias_count])
		++alias_count;

	i->alias_count = alias_count;

	/* copy ip addresses information */
	i->entry.h_addr_list = xcalloc(addr_count + 1, sizeof(char *));
	for (k = 0; k < addr_count; k++) {
	    i->entry.h_addr_list[k] = xcalloc(1, hp->h_length);
	    xmemcpy(i->entry.h_addr_list[k], hp->h_addr_list[k], hp->h_length);
	}

	if (alias_count) {
	    /* copy aliases information */
	    i->entry.h_aliases = xcalloc(alias_count + 1, sizeof(char *));
	    for (k = 0; k < alias_count; k++) {
		i->entry.h_aliases[k] = xcalloc(1, strlen(hp->h_aliases[k]) + 1);
		strcpy(i->entry.h_aliases[k], hp->h_aliases[k]);
	    }
	}
	i->entry.h_length = hp->h_length;
	i->entry.h_name = xstrdup(hp->h_name);
	i->status = IP_CACHED;
	i->ttl = DnsPositiveTtl;
    } else {
	i->status = IP_NEGATIVE_CACHED;
	i->ttl = getNegativeDNSTTL();
    }
    ipcache_add_to_hash(i);
}




/* walks down the pending list, calling handlers */
static void ipcache_call_pending(i)
     ipcache_entry *i;
{
    struct _ip_pending *p = NULL;
    int nhandler = 0;

    i->lastref = squid_curtime;

    while (i->pending_head != NULL) {
	p = i->pending_head;
	i->pending_head = p->next;
	if (p->handler) {
	    nhandler++;
	    dns_error_message = i->error_message;
	    p->handler(p->fd,
		(i->status == IP_CACHED) ? &(i->entry) : NULL,
		p->handlerData);
	}
	memset(p, '\0', sizeof(struct _ip_pending));
	safe_free(p);
    }
    i->pending_head = NULL;	/* nuke list */
    debug(14, 10, "ipcache_call_pending: Called %d handlers.\n", nhandler);
}

static void ipcache_call_pending_badname(fd, handler, data)
     int fd;
     IPH handler;
     void *data;
{
    debug(14, 0, "ipcache_call_pending_badname: Bad Name: Calling handler with NULL result.\n");
    handler(fd, NULL, data);
}

/* free all lines in the list */
static void free_lines(line)
     line_entry *line;
{
    line_entry *tmp;

    while (line) {
	tmp = line;
	line = line->next;
	safe_free(tmp->line);
	safe_free(tmp);
    }
}

/* scan through buffer and do a conversion if possible 
 * return number of char used */
static int ipcache_parsebuffer(buf, offset, dnsData)
     char *buf;
     unsigned int offset;
     dnsserver_t *dnsData;
{
    char *pos = NULL;
    char *tpos = NULL;
    char *endpos = NULL;
    char *token = NULL;
    char *tmp_ptr = NULL;
    line_entry *line_head = NULL;
    line_entry *line_tail = NULL;
    line_entry *line_cur = NULL;
    int ipcount;
    int aliascount;
    ipcache_entry *i = NULL;


    pos = buf;
    while (pos < (buf + offset)) {

	/* no complete record here */
	if ((endpos = strstr(pos, "$end\n")) == NULL) {
	    debug(14, 2, "ipcache_parsebuffer: DNS response incomplete.\n");
	    break;
	}
	line_head = line_tail = NULL;

	while (pos < endpos) {
	    /* add the next line to the end of the list */
	    line_cur = xcalloc(1, sizeof(line_entry));

	    if ((tpos = memchr(pos, '\n', 4096)) == NULL) {
		debug(14, 2, "ipcache_parsebuffer: DNS response incomplete.\n");
		return -1;
	    }
	    *tpos = '\0';
	    line_cur->line = xstrdup(pos);
	    debug(14, 7, "ipcache_parsebuffer: %s\n", line_cur->line);
	    *tpos = '\n';

	    if (line_tail)
		line_tail->next = line_cur;
	    if (line_head == NULL)
		line_head = line_cur;
	    line_tail = line_cur;
	    line_cur = NULL;

	    /* update pointer */
	    pos = tpos + 1;
	}
	pos = endpos + 5;	/* strlen("$end\n") */

	/* 
	 *  At this point, the line_head is a linked list with each
	 *  link node containing another line of the DNS response.
	 *  Start parsing...
	 */
	if (strstr(line_head->line, "$alive")) {
	    dnsData->answer = squid_curtime;
	    free_lines(line_head);
	    debug(14, 10, "ipcache_parsebuffer: $alive succeeded.\n");
	} else if (strstr(line_head->line, "$fail")) {
	    /*
	     *  The $fail messages look like:
	     *      $fail host\n$message msg\n$end\n
	     */
	    token = strtok(line_head->line, w_space);	/* skip first token */
	    if ((token = strtok(NULL, w_space)) == NULL) {
		debug(14, 1, "ipcache_parsebuffer: Invalid $fail?\n");
	    } else {
		line_cur = line_head->next;
		i = dnsData->ip_entry;
		i->ttl = getNegativeDNSTTL();
		i->status = IP_NEGATIVE_CACHED;
		if (line_cur && !strncmp(line_cur->line, "$message", 8))
		    i->error_message = xstrdup(line_cur->line + 8);
		dns_error_message = i->error_message;
		ipcache_call_pending(i);
	    }
	    free_lines(line_head);
	} else if (strstr(line_head->line, "$name")) {
	    tmp_ptr = line_head->line;
	    /* skip the first token */
	    token = strtok(tmp_ptr, w_space);
	    if ((token = strtok(NULL, w_space)) == NULL) {
		debug(14, 0, "ipcache_parsebuffer: Invalid OPCODE?\n");
	    } else {
		i = dnsData->ip_entry;
		if (i->status != IP_DISPATCHED) {
		    debug(14, 0, "ipcache_parsebuffer: DNS record already resolved.\n");
		} else {
		    i->ttl = DnsPositiveTtl;
		    i->status = IP_CACHED;

		    line_cur = line_head->next;

		    /* get $h_name */
		    if (line_cur == NULL ||
			!strstr(line_cur->line, "$h_name")) {
			debug(14, 1, "ipcache_parsebuffer: DNS record in invalid format? No $h_name.\n");
			/* abandon this record */
			break;
		    }
		    tmp_ptr = line_cur->line;
		    /* skip the first token */
		    token = strtok(tmp_ptr, w_space);
		    tmp_ptr = NULL;
		    token = strtok(tmp_ptr, w_space);
		    i->entry.h_name = xstrdup(token);

		    line_cur = line_cur->next;

		    /* get $h_length */
		    if (line_cur == NULL ||
			!strstr(line_cur->line, "$h_len")) {
			debug(14, 1, "ipcache_parsebuffer: DNS record in invalid format? No $h_len.\n");
			/* abandon this record */
			break;
		    }
		    tmp_ptr = line_cur->line;
		    /* skip the first token */
		    token = strtok(tmp_ptr, w_space);
		    tmp_ptr = NULL;
		    token = strtok(tmp_ptr, w_space);
		    i->entry.h_length = atoi(token);

		    line_cur = line_cur->next;

		    /* get $ipcount */
		    if (line_cur == NULL ||
			!strstr(line_cur->line, "$ipcount")) {
			debug(14, 1, "ipcache_parsebuffer: DNS record in invalid format? No $ipcount.\n");
			/* abandon this record */
			break;
		    }
		    tmp_ptr = line_cur->line;
		    /* skip the first token */
		    token = strtok(tmp_ptr, w_space);
		    tmp_ptr = NULL;
		    token = strtok(tmp_ptr, w_space);
		    i->addr_count = ipcount = atoi(token);

		    if (ipcount == 0) {
			i->entry.h_addr_list = NULL;
		    } else {
			i->entry.h_addr_list = xcalloc(ipcount + 1, sizeof(char *));
		    }

		    /* get ip addresses */
		    {
			int k = 0;
			line_cur = line_cur->next;
			while (k < ipcount) {
			    if (line_cur == NULL) {
				debug(14, 1, "ipcache_parsebuffer: DNS record in invalid format? No $ipcount data.\n");
				break;
			    }
			    i->entry.h_addr_list[k] = xcalloc(1, i->entry.h_length);
			    *((u_num32 *) (void *) i->entry.h_addr_list[k]) = inet_addr(line_cur->line);
			    line_cur = line_cur->next;
			    k++;
			}
		    }

		    /* get $aliascount */
		    if (line_cur == NULL ||
			!strstr(line_cur->line, "$aliascount")) {
			debug(14, 1, "ipcache_parsebuffer: DNS record in invalid format? No $aliascount.\n");
			/* abandon this record */
			break;
		    }
		    tmp_ptr = line_cur->line;
		    /* skip the first token */
		    token = strtok(tmp_ptr, w_space);
		    tmp_ptr = NULL;
		    token = strtok(tmp_ptr, w_space);
		    i->alias_count = aliascount = atoi(token);

		    if (aliascount == 0) {
			i->entry.h_aliases = NULL;
		    } else {
			i->entry.h_aliases = xcalloc(aliascount, sizeof(char *));
		    }

		    /* get aliases */
		    {
			int k = 0;
			line_cur = line_cur->next;
			while (k < aliascount) {
			    if (line_cur == NULL) {
				debug(14, 1, "ipcache_parsebuffer: DNS record in invalid format? No $aliascount data.\n");
				break;
			    }
			    i->entry.h_aliases[k] = xstrdup(line_cur->line);
			    line_cur = line_cur->next;
			    k++;
			}
		    }
		    ipcache_call_pending(i);
		    debug(14, 10, "ipcache_parsebuffer: $name succeeded.\n");
		}
	    }
	    free_lines(line_head);
	} else {
	    free_lines(line_head);
	    debug(14, 1, "ipcache_parsebuffer: Invalid OPCODE for DNS table?\n");
	    return -1;
	}
    }
    return (int) (pos - buf);
}


static int ipcache_dnsHandleRead(fd, dnsData)
     int fd;
     dnsserver_t *dnsData;
{
    int char_scanned;
    int len;
    int svc_time;
    int n;
    ipcache_entry *i = NULL;

    len = read(fd,
	dnsData->ip_inbuf + dnsData->offset,
	dnsData->size - dnsData->offset);
    debug(14, 5, "ipcache_dnsHandleRead: Result from DNS ID %d (%d bytes)\n",
	dnsData->id, len);
    if (len <= 0) {
	debug(14, dnsData->flags & DNS_FLAG_CLOSING ? 5 : 1,
	    "FD %d: Connection from DNSSERVER #%d is closed, disabling\n",
	    fd, dnsData->id);
	dnsData->flags = 0;
	comm_set_select_handler(fd,
	    COMM_SELECT_WRITE,
	    NULL,
	    NULL);
	comm_close(fd);
	return 0;
    }
    n = ++IpcacheStats.dnsserver_replies;
    dnsData->offset += len;
    dnsData->ip_inbuf[dnsData->offset] = '\0';

    if (strstr(dnsData->ip_inbuf, "$end\n")) {
	/* end of record found */
	svc_time = tvSubMsec(dnsData->dispatch_time, current_time);
	if (n > IPCACHE_AV_FACTOR)
	    n = IPCACHE_AV_FACTOR;
	IpcacheStats.avg_svc_time
	    = (IpcacheStats.avg_svc_time * (n - 1) + svc_time) / n;
	char_scanned = ipcache_parsebuffer(dnsData->ip_inbuf,
	    dnsData->offset,
	    dnsData);
	if (char_scanned > 0) {
	    /* update buffer */
	    xmemcpy(dnsData->ip_inbuf,
		dnsData->ip_inbuf + char_scanned,
		dnsData->offset - char_scanned);
	    dnsData->offset -= char_scanned;
	    dnsData->ip_inbuf[dnsData->offset] = '\0';
	}
    }
    if (dnsData->offset == 0) {
	dnsData->ip_entry = NULL;
	dnsData->flags &= ~DNS_FLAG_BUSY;
    }
    /* reschedule */
    comm_set_select_handler(dnsData->inpipe,
	COMM_SELECT_READ,
	(PF) ipcache_dnsHandleRead,
	dnsData);
    while ((dnsData = dnsGetFirstAvailable()) && (i = dnsDequeue()))
	dnsDispatch(dnsData, i);
    return 0;
}

static void ipcacheAddPending(i, fd, handler, handlerData)
     ipcache_entry *i;
     int fd;
     IPH handler;
     void *handlerData;
{
    struct _ip_pending *pending = xcalloc(1, sizeof(struct _ip_pending));
    struct _ip_pending **I = NULL;
    i->lastref = squid_curtime;
    pending->fd = fd;
    pending->handler = handler;
    pending->handlerData = handlerData;
    for (I = &(i->pending_head); *I; I = &((*I)->next));
    *I = pending;
}

void ipcache_nbgethostbyname(name, fd, handler, handlerData)
     char *name;
     int fd;
     IPH handler;
     void *handlerData;
{
    ipcache_entry *i = NULL;
    dnsserver_t *dnsData = NULL;
    struct hostent *hp = NULL;

    if (!handler)
	fatal_dump("ipcache_nbgethostbyname: NULL handler");

    debug(14, 4, "ipcache_nbgethostbyname: FD %d: Name '%s'.\n", fd, name);
    IpcacheStats.requests++;

    if (name == NULL || name[0] == '\0') {
	debug(14, 4, "ipcache_nbgethostbyname: Invalid name!\n");
	ipcache_call_pending_badname(fd, handler, handlerData);
	return;
    }
    if ((hp = ipcacheCheckNumeric(name))) {
	handler(fd, hp, handlerData);
	return;
    }
    if ((i = ipcache_get(name))) {
	if (ipcacheExpiredEntry(i)) {
	    ipcache_release(i);
	    i = NULL;
	}
    }
    if (i == NULL) {
	/* MISS: No entry, create the new one */
	debug(14, 5, "ipcache_nbgethostbyname: MISS for '%s'\n", name);
	IpcacheStats.misses++;
	i = ipcache_create();
	i->name = xstrdup(name);
	i->status = IP_PENDING;
	ipcacheAddPending(i, fd, handler, handlerData);
	ipcache_add_to_hash(i);
    } else if (i->status == IP_CACHED || i->status == IP_NEGATIVE_CACHED) {
	/* HIT */
	debug(14, 4, "ipcache_nbgethostbyname: HIT for '%s'\n", name);
	if (i->status == IP_NEGATIVE_CACHED)
	    IpcacheStats.negative_hits++;
	else
	    IpcacheStats.hits++;
	ipcacheAddPending(i, fd, handler, handlerData);
	ipcache_call_pending(i);
	return;
    } else if (i->status == IP_PENDING || i->status == IP_DISPATCHED) {
	debug(14, 4, "ipcache_nbgethostbyname: PENDING for '%s'\n", name);
	IpcacheStats.pending_hits++;
	ipcacheAddPending(i, fd, handler, handlerData);
	return;
    } else {
	fatal_dump("ipcache_nbgethostbyname: BAD ipcache_entry status");
    }

    /* for HIT, PENDING, DISPATCHED we've returned.  For MISS we continue */

    if ((dnsData = dnsGetFirstAvailable()))
	dnsDispatch(dnsData, i);
    else
	dnsEnqueue(i);
}

static void dnsEnqueue(i)
     ipcache_entry *i;
{
    struct dnsQueueData *new = xcalloc(1, sizeof(struct dnsQueueData));
    new->ip_entry = i;
    *dnsQueueTailP = new;
    dnsQueueTailP = &new->next;
}

static ipcache_entry *dnsDequeue()
{
    struct dnsQueueData *old = NULL;
    ipcache_entry *i = NULL;
    if (dnsQueueHead) {
	i = dnsQueueHead->ip_entry;
	old = dnsQueueHead;
	dnsQueueHead = dnsQueueHead->next;
	if (dnsQueueHead == NULL)
	    dnsQueueTailP = &dnsQueueHead;
	safe_free(old);
    }
#ifdef SANITY_CHECK
    if (i == NULL) {
	for (i = ipcache_GetFirst(); i; i = ipcache_GetNext()) {
	    if (i->status == IP_PENDING) {	/* This can't happen */
		debug(14, 0, "IP Cache inconsistency: %s still pending\n",
		    i->name);
	    }
	}
	i = NULL;
    }
#endif
    return i;
}

static dnsserver_t *dnsGetFirstAvailable()
{
    int k;
    dnsserver_t *dns = NULL;
    for (k = 0; k < NDnsServersAlloc; k++) {
	dns = *(dns_child_table + k);
	if (!(dns->flags & DNS_FLAG_BUSY))
	    return dns;
    }
    return NULL;
}

static void dnsDispatch(dns, i)
     dnsserver_t *dns;
     ipcache_entry *i;
{
    char *buf = NULL;
    if (!ipcacheHasPending(i)) {
	debug(14, 0, "dnsDispatch: skipping '%s' because no handler.\n",
	    i->name);
	i->status = IP_NEGATIVE_CACHED;
	ipcache_release(i);
	return;
    }
    i->status = IP_DISPATCHED;
    buf = xcalloc(1, 256);
    sprintf(buf, "%1.254s\n", i->name);
    dns->flags |= DNS_FLAG_BUSY;
    dns->ip_entry = i;
    comm_write(dns->outpipe,
	buf,
	strlen(buf),
	0,			/* timeout */
	NULL,			/* Handler */
	NULL);			/* Handler-data */
    debug(14, 5, "dnsDispatch: Request sent to DNS server #%d.\n",
	dns->id);
    dns->dispatch_time = current_time;
    IpcacheStats.dnsserver_requests++;
    IpcacheStats.dnsserver_hist[dns->id - 1]++;
}


void ipcacheOpenServers()
{
    int N = getDnsChildren();
    char *prg = getDnsProgram();
    int k;
    int dnssocket;
    static char fd_note_buf[FD_ASCII_NOTE_SZ];

    /* free old structures if present */
    if (dns_child_table) {
	for (k = 0; k < NDnsServersAlloc; k++) {
	    safe_free(dns_child_table[k]->ip_inbuf);
	    safe_free(dns_child_table[k]);
	}
	safe_free(dns_child_table);
    }
    dns_child_table = xcalloc(N, sizeof(dnsserver_t *));
    NDnsServersAlloc = N;
    debug(14, 1, "ipcacheOpenServers: Starting %d 'dns_server' processes\n", N);
    for (k = 0; k < N; k++) {
	dns_child_table[k] = xcalloc(1, sizeof(dnsserver_t));
	if ((dnssocket = ipcache_create_dnsserver(prg)) < 0) {
	    debug(14, 1, "ipcacheOpenServers: WARNING: Cannot run 'dnsserver' process.\n");
	    debug(14, 1, "              Fallling back to the blocking version.\n");
	    dns_child_table[k]->flags &= ~DNS_FLAG_ALIVE;
	} else {
	    debug(14, 4, "ipcacheOpenServers: FD %d connected to %s #%d.\n",
		dnssocket, prg, k + 1);
	    dns_child_table[k]->flags |= DNS_FLAG_ALIVE;
	    dns_child_table[k]->id = k + 1;
	    dns_child_table[k]->inpipe = dnssocket;
	    dns_child_table[k]->outpipe = dnssocket;
	    dns_child_table[k]->lastcall = squid_curtime;
	    dns_child_table[k]->size = IP_INBUF_SZ - 1;		/* spare one for \0 */
	    dns_child_table[k]->offset = 0;
	    dns_child_table[k]->ip_inbuf = xcalloc(IP_INBUF_SZ, 1);

	    /* update fd_stat */

	    sprintf(fd_note_buf, "%s #%d", prg, dns_child_table[k]->id);
	    fd_note(dns_child_table[k]->inpipe, fd_note_buf);
	    commSetNonBlocking(dns_child_table[k]->inpipe);

	    /* clear unused handlers */
	    comm_set_select_handler(dns_child_table[k]->inpipe,
		COMM_SELECT_WRITE,
		0,
		0);
	    comm_set_select_handler(dns_child_table[k]->outpipe,
		COMM_SELECT_READ,
		0,
		0);

	    /* set handler for incoming result */
	    comm_set_select_handler(dns_child_table[k]->inpipe,
		COMM_SELECT_READ,
		(PF) ipcache_dnsHandleRead,
		(void *) dns_child_table[k]);
	    debug(14, 3, "ipcacheOpenServers: 'dns_server' %d started\n", k);
	}
    }
}

/* initialize the ipcache */
void ipcache_init()
{

    debug(14, 3, "Initializing IP Cache...\n");

    memset(&IpcacheStats, '\0', sizeof(IpcacheStats));

    /* test naming lookup */
    if (!opt_dns_tests) {
	debug(14, 4, "ipcache_init: Skipping DNS name lookup tests.\n");
    } else if (!ipcache_testname()) {
	fatal("ipcache_init: DNS name lookup tests failed/");
    } else {
	debug(14, 1, "Successful DNS name lookup tests...\n");
    }

    ip_table = hash_create(urlcmp, 229);	/* small hash table */
    /* init static area */
    static_result = xcalloc(1, sizeof(struct hostent));
    static_result->h_length = 4;
    /* Need a terminating NULL address (h_addr_list[1]) */
    static_result->h_addr_list = xcalloc(2, sizeof(char *));
    static_result->h_addr_list[0] = xcalloc(1, 4);
    static_result->h_name = xcalloc(1, MAX_HOST_NAME + 1);

    ipcacheOpenServers();

    ipcache_high = (long) (((float) MAX_IP *
	    (float) IP_HIGH_WATER) / (float) 100);
    ipcache_low = (long) (((float) MAX_IP *
	    (float) IP_LOW_WATER) / (float) 100);
}

/* clean up the pending entries in dnsserver */
/* return 1 if we found the host, 0 otherwise */
int ipcache_unregister(name, fd)
     char *name;
     int fd;
{
    ipcache_entry *i = NULL;
    struct _ip_pending *p = NULL;
    int n = 0;

    debug(14, 3, "ipcache_unregister: FD %d, name '%s'\n", fd, name);
    if ((i = ipcache_get(name)) == NULL)
	return 0;
    if (i->status == IP_PENDING || i->status == IP_DISPATCHED) {
	for (p = i->pending_head; p; p = p->next) {
	    if (p->fd == fd && p->handler != NULL) {
		p->handler = NULL;
		p->fd = -1;
		n++;
	    }
	}
    }
    debug(14, 3, "ipcache_unregister: unregistered %d handlers\n", n);
    return n;
}

struct hostent *ipcache_gethostbyname(name, flags)
     char *name;
     int flags;
{
    ipcache_entry *i = NULL;
    struct hostent *hp = NULL;

    if (!name)
	fatal_dump("ipcache_gethostbyname: NULL name");
    IpcacheStats.requests++;
    if ((i = ipcache_get(name))) {
	if (ipcacheExpiredEntry(i)) {
	    ipcache_release(i);
	    i = NULL;
	}
    }
    if (i) {
	if (i->status == IP_PENDING || i->status == IP_DISPATCHED) {
	    IpcacheStats.pending_hits++;
	    return NULL;
	} else if (i->status == IP_NEGATIVE_CACHED) {
	    IpcacheStats.negative_hits++;
	    dns_error_message = i->error_message;
	    return NULL;
	} else {
	    IpcacheStats.hits++;
	    i->lastref = squid_curtime;
	    return &i->entry;
	}
    }
    IpcacheStats.misses++;
    if ((hp = ipcacheCheckNumeric(name)))
	return hp;
    if (flags & IP_BLOCKING_LOOKUP) {
	IpcacheStats.ghbn_calls++;
	hp = gethostbyname(name);
	if (hp && hp->h_name && (hp->h_name[0] != '\0') && ip_table) {
	    /* good address, cached */
	    ipcache_add(name, ipcache_create(), hp, 1);
	    i = ipcache_get(name);
	    i->lastref = squid_curtime;
	    i->ttl = DnsPositiveTtl;
	    return &i->entry;
	}
	/* bad address, negative cached */
	if (ip_table) {
	    ipcache_add(name, ipcache_create(), hp, 0);
	    i = ipcache_get(name);
	    i->lastref = squid_curtime;
	    i->ttl = getNegativeDNSTTL();
	    return NULL;
	}
    }
    if (flags & IP_LOOKUP_IF_MISS)
	ipcache_nbgethostbyname(name, -1, dummy_handler, NULL);
    return NULL;
}

static void ipcacheStatPrint(i, sentry)
     ipcache_entry *i;
     StoreEntry *sentry;
{
    int ttl;
    int k;
    if (i->status == IP_PENDING || i->status == IP_DISPATCHED)
	ttl = 0;
    else
	ttl = i->ttl + i->lastref - squid_curtime;
    storeAppendPrintf(sentry, " {%-32.32s %c%c %6d %6d %d",
	i->name,
	ipcache_status_char[i->status],
	i->lock ? 'L' : ' ',
	(int) (squid_curtime - i->lastref),
	ttl,
	i->addr_count);
    for (k = 0; k < (int) i->addr_count; k++) {
	struct in_addr addr;
	xmemcpy(&addr, i->entry.h_addr_list[k], i->entry.h_length);
	storeAppendPrintf(sentry, " %15s", inet_ntoa(addr));
    }
    for (k = 0; k < (int) i->alias_count; k++) {
	storeAppendPrintf(sentry, " %s", i->entry.h_aliases[k]);
    }
    if (i->entry.h_name && strncmp(i->name, i->entry.h_name, MAX_LINELEN)) {
	storeAppendPrintf(sentry, " %s", i->entry.h_name);
    }
    storeAppendPrintf(sentry, close_bracket);
}

/* process objects list */
void stat_ipcache_get(sentry)
     StoreEntry *sentry;
{
    int k;
    int N;
    ipcache_entry *i = NULL;
    ipcache_entry **list = NULL;
    if (!ip_table)
	return;
    storeAppendPrintf(sentry, "{IP Cache Statistics:\n");
    storeAppendPrintf(sentry, "{IPcache Entries: %d}\n",
	meta_data.ipcache_count);
    storeAppendPrintf(sentry, "{IPcache Requests: %d}\n",
	IpcacheStats.requests);
    storeAppendPrintf(sentry, "{IPcache Hits: %d}\n",
	IpcacheStats.hits);
    storeAppendPrintf(sentry, "{IPcache Pending Hits: %d}\n",
	IpcacheStats.pending_hits);
    storeAppendPrintf(sentry, "{IPcache Negative Hits: %d}\n",
	IpcacheStats.negative_hits);
    storeAppendPrintf(sentry, "{IPcache Misses: %d}\n",
	IpcacheStats.misses);
    storeAppendPrintf(sentry, "{Blocking calls to gethostbyname(): %d}\n",
	IpcacheStats.ghbn_calls);
    storeAppendPrintf(sentry, "{dnsserver requests: %d}\n",
	IpcacheStats.dnsserver_requests);
    storeAppendPrintf(sentry, "{dnsserver replies: %d}\n",
	IpcacheStats.dnsserver_replies);
    storeAppendPrintf(sentry, "{dnsserver avg service time: %d msec}\n",
	IpcacheStats.avg_svc_time);
    storeAppendPrintf(sentry, "{number of dnsservers: %d}\n",
	getDnsChildren());
    storeAppendPrintf(sentry, "{dnsservers use histogram:}\n");
    for (k = 0; k < getDnsChildren(); k++) {
	storeAppendPrintf(sentry, "{    dnsserver #%d: %d}\n",
	    k + 1,
	    IpcacheStats.dnsserver_hist[k]);
    }
    storeAppendPrintf(sentry, "}\n\n");
    storeAppendPrintf(sentry, "{IP Cache Contents:\n\n");
    storeAppendPrintf(sentry, " {%-29.29s %5s %6s %6s %1s}\n",
	"Hostname",
	"Flags",
	"lstref",
	"TTL",
	"N");
    list = xcalloc(meta_data.ipcache_count, sizeof(ipcache_entry *));
    N = 0;
    for (i = ipcache_GetFirst(); i; i = ipcache_GetNext()) {
	*(list + N) = i;
	if (++N > meta_data.ipcache_count)
	    fatal_dump("stat_ipcache_get: meta_data.ipcache_count mismatch");
    }
    qsort((char *) list,
	N,
	sizeof(ipcache_entry *),
	(QS) ipcache_reverseLastRef);
    for (k = 0; k < N; k++)
	ipcacheStatPrint(*(list + k), sentry);
    storeAppendPrintf(sentry, close_bracket);
}

void ipcacheShutdownServers()
{
    dnsserver_t *dnsData = NULL;
    int k;
    static char *shutdown = "$shutdown\n";

    debug(14, 3, "ipcacheShutdownServers:\n");

    for (k = 0; k < getDnsChildren(); k++) {
	dnsData = *(dns_child_table + k);
	if (!(dnsData->flags & DNS_FLAG_ALIVE))
	    continue;
	if (dnsData->flags & DNS_FLAG_BUSY)
	    continue;
	if (dnsData->flags & DNS_FLAG_CLOSING)
	    continue;
	debug(14, 3, "ipcacheShutdownServers: sending '$shutdown' to dnsserver #%d\n", dnsData->id);
	debug(14, 3, "ipcacheShutdownServers: --> FD %d\n", dnsData->outpipe);
	comm_write(dnsData->outpipe,
	    xstrdup(shutdown),
	    strlen(shutdown),
	    0,			/* timeout */
	    NULL,		/* Handler */
	    NULL);		/* Handler-data */
	dnsData->flags |= DNS_FLAG_CLOSING;
    }
}

static int dummy_handler(u1, u2, u3)
     int u1;
     struct hostent *u2;
     void *u3;
{
    return 0;
}

void ipcacheLockEntry(name)
     char *name;
{
    ipcache_entry *i;
    if ((i = ipcache_get(name)) == NULL)
	return;
    i->lock++;
}

static int ipcacheHasPending(i)
     ipcache_entry *i;
{
    struct _ip_pending *p = NULL;
    if (i->status != IP_PENDING)
	return 0;
    for (p = i->pending_head; p; p = p->next)
	if (p->handler)
	    return 1;
    return 0;
}

static struct hostent *ipcacheCheckNumeric(name)
     char *name;
{
    unsigned int ip;
    /* check if it's already a IP address in text form. */
    if ((ip = inet_addr(name)) == INADDR_NONE)
	return NULL;
    *((u_num32 *) (void *) static_result->h_addr_list[0]) = ip;
    strncpy(static_result->h_name, name, MAX_HOST_NAME);
    return static_result;
}

/*
 * $Id$
 *
 * DEBUG: section 28    Access Control
 * AUTHOR: Duane Wessels
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

#include "squid.h"

/* These two should never be referenced directly in this file! */
struct _acl_access *HTTPAccessList = NULL;
struct _acl_access *ICPAccessList = NULL;

static struct _acl *AclList = NULL;
static struct _acl **AclListTail = &AclList;

static void aclDestroyAclList _PARAMS((struct _acl_list * list));
static void aclDestroyIpList _PARAMS((struct _acl_ip_data * data));
static void aclDestroyRegexList _PARAMS((struct _relist * data));
static void aclDestroyTimeList _PARAMS((struct _acl_time_data * data));
static int aclMatchAclList _PARAMS((struct _acl_list *, aclCheck_t *));
static int aclMatchInteger _PARAMS((intlist * data, int i));
static int aclMatchIp _PARAMS((struct _acl_ip_data * data, struct in_addr c));
static int aclMatchRegex _PARAMS((relist * data, char *word));
static int aclMatchTime _PARAMS((struct _acl_time_data * data, time_t when));
static int aclMatchEndOfWord _PARAMS((wordlist * data, char *word));
static intlist *aclParseIntlist _PARAMS((void));
static struct _acl_ip_data *aclParseIpList _PARAMS((void));
static intlist *aclParseMethodList _PARAMS((void));
static intlist *aclParseProtoList _PARAMS((void));
static struct _relist *aclParseRegexList _PARAMS((void));
static struct _acl_time_data *aclParseTimeSpec _PARAMS((void));
static wordlist *aclParseWordList _PARAMS((void));
static squid_acl aclType _PARAMS((char *s));

static squid_acl aclType(s)
     char *s;
{
    if (!strcmp(s, "src"))
	return ACL_SRC_IP;
    if (!strcmp(s, "dst"))
	return ACL_DST_IP;
    if (!strcmp(s, "domain"))
	return ACL_DST_DOMAIN;
    if (!strcmp(s, "time"))
	return ACL_TIME;
    if (!strcmp(s, "pattern"))
	return ACL_URL_REGEX;
    if (!strcmp(s, "port"))
	return ACL_URL_PORT;
    if (!strcmp(s, "user"))
	return ACL_USER;
    if (!strncmp(s, "proto", 5))
	return ACL_PROTO;
    if (!strcmp(s, "method"))
	return ACL_METHOD;
    return ACL_NONE;
}

struct _acl *aclFindByName(name)
     char *name;
{
    struct _acl *a;
    for (a = AclList; a; a = a->next)
	if (!strcasecmp(a->name, name))
	    return a;
    return NULL;
}


static intlist *aclParseIntlist()
{
    intlist *head = NULL;
    intlist **Tail = &head;
    intlist *q = NULL;
    char *t = NULL;
    while ((t = strtok(NULL, w_space))) {
	q = xcalloc(1, sizeof(intlist));
	q->i = atoi(t);
	*(Tail) = q;
	Tail = &q->next;
    }
    return head;
}

static intlist *aclParseProtoList()
{
    intlist *head = NULL;
    intlist **Tail = &head;
    intlist *q = NULL;
    char *t = NULL;
    while ((t = strtok(NULL, w_space))) {
	q = xcalloc(1, sizeof(intlist));
	q->i = (int) urlParseProtocol(t);
	*(Tail) = q;
	Tail = &q->next;
    }
    return head;
}

static intlist *aclParseMethodList()
{
    intlist *head = NULL;
    intlist **Tail = &head;
    intlist *q = NULL;
    char *t = NULL;
    while ((t = strtok(NULL, w_space))) {
	q = xcalloc(1, sizeof(intlist));
	q->i = (int) urlParseMethod(t);
	*(Tail) = q;
	Tail = &q->next;
    }
    return head;
}

/* Decode a ascii representation (asc) of a IP adress, and place
 * adress and netmask information in addr and mask.
 */
static int decode_addr(asc, addr, mask)
     char *asc;
     struct in_addr *addr, *mask;
{
    struct hostent *hp = NULL;
    u_num32 a;
    int a1, a2, a3, a4;

    switch (sscanf(asc, "%d.%d.%d.%d", &a1, &a2, &a3, &a4)) {
    case 4:			/* a dotted quad */
	if ((a = inet_addr(asc)) != INADDR_NONE ||
	    !strcmp(asc, "255.255.255.255")) {
	    addr->s_addr = a;
	    /* inet_addr() outputs in network byte order */
	}
	break;
    case 1:			/* a significant bits value for a mask */
	if (a1 >= 0 && a1 < 33) {
	    addr->s_addr = htonl(0xffffffff << (32 - a1));
	    break;
	}
    default:
	if ((hp = gethostbyname(asc)) != NULL) {
	    /* We got a host name */
	    xmemcpy(addr, hp->h_addr, hp->h_length);
	} else {
	    /* XXX: Here we could use getnetbyname */
	    debug(28, 0, "decode_addr: Invalid IP address or hostname  '%s'\n", asc);
	    return 0;		/* This is not valid address */
	}
	break;
    }

    if (mask != NULL) {		/* mask == NULL if called to decode a netmask */

	/* Guess netmask */
	a = ntohl(addr->s_addr);
	if (!(a & 0xFFFFFFFF))
	    mask->s_addr = htonl(0x00000000);
	else if (!(a & 0x00FFFFFF))
	    mask->s_addr = htonl(0xFF000000);
	else if (!(a & 0x0000FFFF))
	    mask->s_addr = htonl(0xFFFF0000);
	else if (!(a & 0x000000FF))
	    mask->s_addr = htonl(0xFFFFFF00);
	else
	    mask->s_addr = htonl(0xFFFFFFFF);
    }
    return 1;
}


static struct _acl_ip_data *aclParseIpList()
{
    char *t = NULL, *p = NULL;
    struct _acl_ip_data *head = NULL;
    struct _acl_ip_data **Tail = &head;
    struct _acl_ip_data *q = NULL;
    LOCAL_ARRAY(char, addr1, 256);
    LOCAL_ARRAY(char, addr2, 256);
    LOCAL_ARRAY(char, mask, 256);

    while ((t = strtok(NULL, w_space))) {
	q = xcalloc(1, sizeof(struct _acl_ip_data));
	if (!strcasecmp(t, "all")) {
	    q->addr1.s_addr = 0;
	    q->addr2.s_addr = 0;
	    q->mask.s_addr = 0;
	} else {
	    p = t;
	    memset(addr1, 0, sizeof(addr1));
	    memset(addr2, 0, sizeof(addr2));
	    memset(mask, 0, sizeof(mask));

	    /* Split the adress in addr1-addr2/mask */
	    strncpy(addr1, p, strcspn(t, "-/"));
	    p += strcspn(t, "-/");
	    if (*p == '-') {
		p++;
		strncpy(addr2, p, strcspn(t, "/"));
		p += strcspn(p, "/");
	    }
	    if (*p == '/') {
		p++;
		strcpy(mask, p);
	    }
	    /* Decode addr1 */
	    if (!decode_addr(addr1, &q->addr1, &q->mask)) {
		debug(28, 0, "%s line %d: %s\n",
		    cfg_filename, config_lineno, config_input_line);
		debug(28, 0, "aclParseIpList: Ignoring invalid IP acl entry: unknown first address '%s'\n", addr1);
		safe_free(q);
		continue;
	    }
	    /* Decode addr2 */
	    if (*addr2 && !decode_addr(addr2, &q->addr2, &q->mask)) {
		debug(28, 0, "%s line %d: %s\n",
		    cfg_filename, config_lineno, config_input_line);
		debug(28, 0, "aclParseIpList: Ignoring invalid IP acl entry: unknown second address '%s'\n", addr1);
		safe_free(q);
		continue;
	    }
	    /* Decode mask */
	    if (*mask && !decode_addr(mask, &q->mask, NULL)) {
		debug(28, 0, "%s line %d: %s\n",
		    cfg_filename, config_lineno, config_input_line);
		debug(28, 0, "aclParseIpList: Ignoring invalid IP acl entry: unknown netmask '%s'\n", mask);
		safe_free(q);
		continue;
	    }
	}
	*(Tail) = q;
	Tail = &q->next;
    }
    return head;
}

static struct _acl_time_data *aclParseTimeSpec()
{
    struct _acl_time_data *data = NULL;
    int h1, m1, h2, m2;
    char *t = NULL;

    data = xcalloc(1, sizeof(struct _acl_time_data));
    while ((t = strtok(NULL, w_space))) {
	if (*t < '0' || *t > '9') {
	    /* assume its day-of-week spec */
	    while (*t) {
		switch (*t++) {
		case 'S':
		    data->weekbits |= ACL_SUNDAY;
		    break;
		case 'M':
		    data->weekbits |= ACL_MONDAY;
		    break;
		case 'T':
		    data->weekbits |= ACL_TUESDAY;
		    break;
		case 'W':
		    data->weekbits |= ACL_WEDNESDAY;
		    break;
		case 'H':
		    data->weekbits |= ACL_THURSDAY;
		    break;
		case 'F':
		    data->weekbits |= ACL_FRIDAY;
		    break;
		case 'A':
		    data->weekbits |= ACL_SATURDAY;
		    break;
		case 'D':
		    data->weekbits |= ACL_WEEKDAYS;
		    break;
		default:
		    debug(28, 0, "%s line %d: %s\n",
			cfg_filename, config_lineno, config_input_line);
		    debug(28, 0, "aclParseTimeSpec: Bad Day '%c'\n",
			*t);
		    break;
		}
	    }
	} else {
	    /* assume its time-of-day spec */
	    if (sscanf(t, "%d:%d-%d:%d", &h1, &m1, &h2, &m2) < 4) {
		debug(28, 0, "%s line %d: %s\n",
		    cfg_filename, config_lineno, config_input_line);
		debug(28, 0, "aclParseTimeSpec: Bad time range '%s'\n",
		    t);
		xfree(&data);
		return NULL;
	    }
	    data->start = h1 * 60 + m1;
	    data->stop = h2 * 60 + m2;
	    if (data->start > data->stop) {
		debug(28, 0, "%s line %d: %s\n",
		    cfg_filename, config_lineno, config_input_line);
		debug(28, 0, "aclParseTimeSpec: Reversed time range '%s'\n",
		    t);
		xfree(&data);
		return NULL;
	    }
	}
    }
    if (data->start == 0 && data->stop == 0)
	data->stop = 23 * 60 + 59;
    if (data->weekbits == 0)
	data->weekbits = ACL_ALLWEEK;
    return data;
}

static struct _relist *aclParseRegexList()
{
    relist *head = NULL;
    relist **Tail = &head;
    relist *q = NULL;
    char *t = NULL;
    regex_t comp;
    while ((t = strtok(NULL, w_space))) {
	if (regcomp(&comp, t, REG_EXTENDED) != REG_NOERROR) {
	    debug(28, 0, "%s line %d: %s\n",
		cfg_filename, config_lineno, config_input_line);
	    debug(28, 0, "aclParseRegexList: Invalid regular expression: '%s'\n", t);
	    continue;
	}
	q = xcalloc(1, sizeof(relist));
	q->pattern = xstrdup(t);
	q->regex = comp;
	*(Tail) = q;
	Tail = &q->next;
    }
    return head;
}

static wordlist *aclParseWordList()
{
    wordlist *head = NULL;
    wordlist **Tail = &head;
    wordlist *q = NULL;
    char *t = NULL;
    while ((t = strtok(NULL, w_space))) {
	q = xcalloc(1, sizeof(wordlist));
	q->key = xstrdup(t);
	*(Tail) = q;
	Tail = &q->next;
    }
    return head;
}



void aclParseAclLine()
{
    /* we're already using strtok() to grok the line */
    char *t = NULL;
    struct _acl *A = NULL;

    A = xcalloc(1, sizeof(struct _acl));
    /* snarf the ACL name */
    if ((t = strtok(NULL, w_space)) == NULL) {
	debug(28, 0, "%s line %d: %s\n",
	    cfg_filename, config_lineno, config_input_line);
	debug(28, 0, "aclParseAclLine: missing ACL name.\n");
	xfree(A);
	return;
    }
    if (aclFindByName(t)) {
	debug(28, 0, "%s line %d: %s\n",
	    cfg_filename, config_lineno, config_input_line);
	debug(28, 0, "aclParseAclLine: ACL name '%s' already exists.\n", t);
	xfree(A);
	return;
    }
    strncpy(A->name, t, ACL_NAME_SZ);
    /* snarf the ACL type */
    if ((t = strtok(NULL, w_space)) == NULL) {
	xfree(A);
	debug(28, 0, "%s line %d: %s\n",
	    cfg_filename, config_lineno, config_input_line);
	debug(28, 0, "aclParseAclLine: missing ACL type.\n");
	return;
    }
    switch (A->type = aclType(t)) {
    case ACL_SRC_IP:
    case ACL_DST_IP:
	A->data = (void *) aclParseIpList();
	break;
    case ACL_DST_DOMAIN:
	A->data = (void *) aclParseWordList();
	break;
    case ACL_TIME:
	A->data = (void *) aclParseTimeSpec();
	break;
    case ACL_URL_REGEX:
	A->data = (void *) aclParseRegexList();
	break;
    case ACL_URL_PORT:
	A->data = (void *) aclParseIntlist();
	break;
    case ACL_USER:
	A->data = (void *) aclParseWordList();
	break;
    case ACL_PROTO:
	A->data = (void *) aclParseProtoList();
	break;
    case ACL_METHOD:
	A->data = (void *) aclParseMethodList();
	break;
    case ACL_NONE:
    default:
	debug(28, 0, "%s line %d: %s\n",
	    cfg_filename, config_lineno, config_input_line);
	debug(28, 0, "aclParseAclLine: Invalid ACL type '%s'\n", t);
	xfree(A);
	return;
	/* NOTREACHED */
    }
    A->cfgline = xstrdup(config_input_line);
    *AclListTail = A;
    AclListTail = &A->next;
}

void aclParseAccessLine(head)
     struct _acl_access **head;
{
    char *t = NULL;
    struct _acl_access *A = NULL;
    struct _acl_access *B = NULL;
    struct _acl_access **T = NULL;
    struct _acl_list *L = NULL;
    struct _acl_list **Tail = NULL;
    struct _acl *a = NULL;

    /* first expect either 'allow' or 'deny' */
    if ((t = strtok(NULL, w_space)) == NULL) {
	debug(28, 0, "%s line %d: %s\n",
	    cfg_filename, config_lineno, config_input_line);
	debug(28, 0, "aclParseAccessLine: missing 'allow' or 'deny'.\n");
	return;
    }
    A = xcalloc(1, sizeof(struct _acl_access));
    if (!strcmp(t, "allow"))
	A->allow = 1;
    else if (!strcmp(t, "deny"))
	A->allow = 0;
    else {
	debug(28, 0, "%s line %d: %s\n",
	    cfg_filename, config_lineno, config_input_line);
	debug(28, 0, "aclParseAccessLine: expecting 'allow' or 'deny', got '%s'.\n", t);
	xfree(A);
	return;
    }

    /* next expect a list of ACL names, possibly preceeded
     * by '!' for negation */
    Tail = &A->acl_list;
    while ((t = strtok(NULL, w_space))) {
	L = xcalloc(1, sizeof(struct _acl_list));
	L->op = 1;		/* defaults to non-negated */
	if (*t == '!') {
	    /* negated ACL */
	    L->op = 0;
	    t++;
	}
	debug(28, 3, "aclParseAccessLine: looking for ACL name '%s'\n", t);
	a = aclFindByName(t);
	if (a == NULL) {
	    debug(28, 0, "%s line %d: %s\n",
		cfg_filename, config_lineno, config_input_line);
	    debug(28, 0, "aclParseAccessLine: ACL name '%s' not found.\n", t);
	    xfree(L);
	    continue;
	}
	L->acl = a;
	*Tail = L;
	Tail = &L->next;
    }
    if (A->acl_list == NULL) {
	debug(28, 0, "%s line %d: %s\n",
	    cfg_filename, config_lineno, config_input_line);
	debug(28, 0, "aclParseAccessLine: Access line contains no ACL's, skipping\n");
	xfree(A);
	return;
    }
    A->cfgline = xstrdup(config_input_line);
    for (B = *head, T = head; B; T = &B->next, B = B->next);	/* find the tail */
    *T = A;
}

static int aclMatchIp(data, c)
     struct _acl_ip_data *data;
     struct in_addr c;
{
    struct in_addr h;
    unsigned long lh, la1, la2;

    while (data) {
	h.s_addr = c.s_addr & data->mask.s_addr;
	debug(28, 3, "aclMatchIp: h     = %s\n", inet_ntoa(h));
	debug(28, 3, "aclMatchIp: addr1 = %s\n", inet_ntoa(data->addr1));
	debug(28, 3, "aclMatchIp: addr2 = %s\n", inet_ntoa(data->addr2));
	if (!data->addr2.s_addr) {
	    if (h.s_addr == data->addr1.s_addr) {
		debug(28, 3, "aclMatchIp: returning 1\n");
		return 1;
	    }
	} else {
	    /* This is a range check */
	    lh = ntohl(h.s_addr);
	    la1 = ntohl(data->addr1.s_addr);
	    la2 = ntohl(data->addr2.s_addr);
	    if (lh >= la1 && lh <= la2) {
		debug(28, 3, "aclMatchIp: returning 1\n");
		return 1;
	    }
	}
	data = data->next;
    }
    debug(28, 3, "aclMatchIp: returning 0\n");
    return 0;
}

#ifdef UNUSED_CODE
static int aclMatchWord(data, word)
     wordlist *data;
     char *word;
{
    if (word == NULL)
	return 0;
    debug(28, 3, "aclMatchWord: checking '%s'\n", word);
    while (data) {
	debug(28, 3, "aclMatchWord: looking for '%s'\n", data->key);
	if (strstr(word, data->key))
	    return 1;
	data = data->next;
    }
    return 0;
}
#endif

static int aclMatchEndOfWord(data, word)
     wordlist *data;
     char *word;
{
    int offset;
    if (word == NULL)
	return 0;
    debug(28, 3, "aclMatchEndOfWord: checking '%s'\n", word);
    for (; data; data = data->next) {
	debug(28, 3, "aclMatchEndOfWord: looking for '%s'\n", data->key);
	if ((offset = strlen(word) - strlen(data->key)) < 0)
	    continue;
	if (strcasecmp(word + offset, data->key) == 0)
	    return 1;
    }
    return 0;
}

static int aclMatchRegex(data, word)
     relist *data;
     char *word;
{
    if (word == NULL)
	return 0;
    debug(28, 3, "aclMatchRegex: checking '%s'\n", word);
    while (data) {
	debug(28, 3, "aclMatchRegex: looking for '%s'\n", data->pattern);
	if (regexec(&data->regex, word, 0, 0, 0) == 0)
	    return 1;
	data = data->next;
    }
    return 0;
}

static int aclMatchInteger(data, i)
     intlist *data;
     int i;
{
    while (data) {
	if (data->i == i)
	    return 1;
	data = data->next;
    }
    return 0;
}

static int aclMatchTime(data, when)
     struct _acl_time_data *data;
     time_t when;
{
    static time_t last_when = 0;
    static struct tm tm;
    time_t t;

    if (when != last_when) {
	last_when = when;
	xmemcpy(&tm, localtime(&when), sizeof(struct tm));
    }
    t = (time_t) (tm.tm_hour * 60 + tm.tm_min);
    debug(28, 3, "aclMatchTime: checking %d in %d-%d, weekbits=%x\n",
	(int) t, (int) data->start, (int) data->stop, data->weekbits);

    if (t < data->start || t > data->stop)
	return 0;
    return data->weekbits & (1 << tm.tm_wday) ? 1 : 0;
}

int aclMatchAcl(acl, checklist)
     struct _acl *acl;
     aclCheck_t *checklist;
{
    request_t *r = checklist->request;
    struct hostent *hp = NULL;
    int k;
    if (!acl)
	return 0;
    debug(28, 3, "aclMatchAcl: checking '%s'\n", acl->cfgline);
    switch (acl->type) {
    case ACL_SRC_IP:
	return aclMatchIp(acl->data, checklist->src_addr);
	/* NOTREACHED */
    case ACL_DST_IP:
	if ((hp = ipcache_gethostbyname(r->host, IP_LOOKUP_IF_MISS)) == NULL) {
	    debug(28, 3, "aclMatchAcl: Can't yet compare '%s' ACL for '%s'\n",
		acl->name, r->host);
	    return 0;		/* cant check, return no match */
	}
	for (k = 0; *(hp->h_addr_list + k); k++) {
	    xmemcpy(&checklist->dst_addr.s_addr,
		*(hp->h_addr_list + k),
		hp->h_length);
	    if (aclMatchIp(acl->data, checklist->dst_addr))
		return 1;
	}
	return 0;
	/* NOTREACHED */
    case ACL_DST_DOMAIN:
	/* XXX This probably needs to use matchDomainName() */
	return aclMatchEndOfWord(acl->data, r->host);
	/* NOTREACHED */
    case ACL_TIME:
	return aclMatchTime(acl->data, squid_curtime);
	/* NOTREACHED */
    case ACL_URL_REGEX:
	return aclMatchRegex(acl->data, r->urlpath);
	/* NOTREACHED */
    case ACL_URL_PORT:
	return aclMatchInteger(acl->data, r->port);
	/* NOTREACHED */
    case ACL_USER:
	debug(28, 0, "aclMatchAcl: ACL_USER unimplemented\n");
	return 0;
	/* NOTREACHED */
    case ACL_PROTO:
	return aclMatchInteger(acl->data, r->protocol);
	/* NOTREACHED */
    case ACL_METHOD:
	return aclMatchInteger(acl->data, r->method);
	/* NOTREACHED */
    case ACL_NONE:
    default:
	debug(28, 0, "aclMatchAcl: '%s' has bad type %d\n",
	    acl->name, acl->type);
	return 0;
    }
    /* NOTREACHED */
}

static int aclMatchAclList(list, checklist)
     struct _acl_list *list;
     aclCheck_t *checklist;
{
    debug(28, 3, "aclMatchAclList: list=%p  op=%d\n", list, list->op);
    while (list) {
	if (aclMatchAcl(list->acl, checklist) != list->op) {
	    debug(28, 3, "aclMatchAclList: returning 0\n");
	    return 0;
	}
	list = list->next;
    }
    debug(28, 3, "aclMatchAclList: returning 1\n");
    return 1;
}

int aclCheck(A, checklist)
     struct _acl_access *A;
     aclCheck_t *checklist;
{
    int allow = 0;

    while (A) {
	debug(28, 3, "aclCheck: checking '%s'\n", A->cfgline);
	allow = A->allow;
	if (aclMatchAclList(A->acl_list, checklist)) {
	    debug(28, 3, "aclCheck: match found, returning %d\n", allow);
	    return allow;
	}
	A = A->next;
    }
    return !allow;
}

static void aclDestroyIpList(data)
     struct _acl_ip_data *data;
{
    struct _acl_ip_data *next;
    for (; data; data = next) {
	next = data->next;
	safe_free(data);
    }
}

static void aclDestroyTimeList(data)
     struct _acl_time_data *data;
{
    struct _acl_time_data *next;
    for (; data; data = next) {
	next = data->next;
	safe_free(data);
    }
}

static void aclDestroyRegexList(data)
     struct _relist *data;
{
    struct _relist *next;
    for (; data; data = next) {
	next = data->next;
	regfree(&data->regex);
	safe_free(data->pattern);
	safe_free(data);
    }
}

void aclDestroyAcls()
{
    struct _acl *a = NULL;
    struct _acl *next = NULL;
    for (a = AclList; a; a = next) {
	next = a->next;
	debug(28, 3, "aclDestroyAcls: '%s'\n", a->cfgline);
	switch (a->type) {
	case ACL_SRC_IP:
	case ACL_DST_IP:
	    aclDestroyIpList(a->data);
	    break;
	case ACL_DST_DOMAIN:
	case ACL_USER:
	    wordlistDestroy((wordlist **) & a->data);
	    break;
	case ACL_TIME:
	    aclDestroyTimeList(a->data);
	    break;
	case ACL_URL_REGEX:
	    aclDestroyRegexList(a->data);
	    break;
	case ACL_URL_PORT:
	case ACL_PROTO:
	case ACL_METHOD:
	    intlistDestroy((intlist **) & a->data);
	    break;
	case ACL_NONE:
	default:
	    fatal_dump("aclDestroyAcls: Found ACL_NONE?");
	    break;
	}
	safe_free(a->cfgline);
	safe_free(a);
    }
    AclList = NULL;
    AclListTail = &AclList;
}

void aclDestroyAclList(list)
     struct _acl_list *list;
{
    struct _acl_list *next = NULL;
    for (; list; list = next) {
	next = list->next;
	safe_free(list);
    }
}

void aclDestroyAccessList(list)
     struct _acl_access **list;
{
    struct _acl_access *l = NULL;
    struct _acl_access *next = NULL;
    for (l = *list; l; l = next) {
	debug(28, 3, "aclDestroyAccessList: '%s'\n", l->cfgline);
	next = l->next;
	aclDestroyAclList(l->acl_list);
	l->acl_list = NULL;
	safe_free(l->cfgline);
	safe_free(l);
    }
    *list = NULL;
}


/*
 * $Id$
 *
 * DEBUG: Section 0     CGI Cache Manager
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

#include "config.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_GRP_H
#include <grp.h>
#endif
#if HAVE_MALLOC_H && !defined(_SQUID_FREEBSD_) && !defined(_SQUID_NEXT_)
#include <malloc.h>
#endif
#if HAVE_MEMORY_H
#include <memory.h>
#endif
#if HAVE_NETDB_H && !defined(_SQUID_NETDB_H_)	/* protect NEXTSTEP */
#define _SQUID_NETDB_H_
#include <netdb.h>
#endif
#if HAVE_PWD_H
#include <pwd.h>
#endif
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#if HAVE_TIME_H
#include <time.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_RESOURCE_H
#include <sys/resource.h>	/* needs sys/time.h above it */
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if HAVE_LIBC_H
#include <libc.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STRINGS_H
#include <strings.h>
#endif
#if HAVE_BSTRING_H
#include <bstring.h>
#endif
#if HAVE_CRYPT_H
#include <crypt.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "ansiproto.h"
#include "util.h"

#define MAX_ENTRIES 10000

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define LF 10
#define CR 13

typedef enum {
    INFO,
    CACHED,
    SERVER,
    LOG,
    PARAM,
    STATS_I,
    STATS_F,
    STATS_D,
    STATS_R,
    STATS_O,
    STATS_VM,
    STATS_U,
    STATS_IO,
    STATS_HDRS,
    STATS_FDS,
    SHUTDOWN,
    REFRESH,
#ifdef REMOVE_OBJECT
    REMOVE,
#endif
    MAXOP
} op_t;

static char *op_cmds[] =
{
    "info",
    "squid.conf",
    "server_list",
    "log",
    "parameter",
    "stats/ipcache",
    "stats/fqdncache",
    "stats/dns",
    "stats/redirector",
    "stats/objects",
    "stats/vm_objects",
    "stats/utilization",
    "stats/io",
    "stats/reply_headers",
    "stats/filedescriptors",
    "shutdown",
    "<refresh>",
#ifdef REMOVE_OBJECT
    "<remove>",
#endif
    "<maxop>"
};

typedef struct {
    char *name;
    char *val;
} entry;

int hasTables = FALSE;

char *script_name = "/cgi-bin/cachemgr.cgi";
char *progname = NULL;

static int client_comm_connect __P((int sock, char *dest_host, u_short dest_port));

void
print_trailer(void)
{
    time_t now = time(NULL);
    static char tbuf[128];
    struct tm *gmt;

    gmt = gmtime(&now);
    strftime(tbuf, 128, "%A, %d-%b-%y %H:%M:%S GMT", gmt);

    printf("<HR>\n");
    printf("<ADDRESS>\n");
    printf("Generated %s, by %s/%s@%s\n",
	tbuf, progname, SQUID_VERSION, getfullhostname());
    printf("</ADDRESS></BODY></HTML>\n");
}

void
noargs_html(char *host, int port)
{
    printf("\r\n\r\n");
    printf("<HTML><HEAD><TITLE>Cache Manager Interface</TITLE></HEAD>\n");
    printf("<BODY><H1>Cache Manager Interface</H1>\n");
    printf("<p>This is a WWW interface to the instrumentation interface\n");
    printf("for the Squid object cache.</p>\n");
    printf("<HR>\n");
    printf("<PRE>\n");
    printf("<FORM METHOD=\"POST\" ACTION=\"%s?%s:%d\">\n",
	script_name, host, port);
    printf("<STRONG>Cache Host:</STRONG><INPUT NAME=\"host\" ");
    printf("SIZE=30 VALUE=\"%s\">\n\n", host);
    printf("<STRONG>Cache Port:</STRONG><INPUT NAME=\"port\" ");
    printf("SIZE=30 VALUE=\"%d\">\n\n", port);
    printf("<STRONG>Password  :</STRONG><INPUT TYPE=\"password\" ");
    printf("NAME=\"password\" SIZE=30 VALUE=\"\">\n\n");
    printf("<STRONG>URL       :</STRONG><INPUT NAME=\"url\" ");
    printf("SIZE=30 VALUE=\"\">\n\n");
    printf("<STRONG>Operation :</STRONG>");
    printf("<SELECT NAME=\"operation\">\n");
    printf("<OPTION SELECTED VALUE=\"info\">Cache Information\n");
    printf("<OPTION VALUE=\"squid.conf\">Cache Configuration File\n");
    printf("<OPTION VALUE=\"parameter\">Cache Parameters\n");
#ifdef MENU_SHOW_LOG
    printf("<OPTION VALUE=\"log\">Cache Log\n");
#endif
    printf("<OPTION VALUE=\"stats/utilization\">Utilization\n");
    printf("<OPTION VALUE=\"stats/io\">I/O\n");
    printf("<OPTION VALUE=\"stats/reply_headers\">HTTP Reply Headers\n");
    printf("<OPTION VALUE=\"stats/filedescriptors\">Filedescriptor Usage\n");
    printf("<OPTION VALUE=\"stats/objects\">Objects\n");
    printf("<OPTION VALUE=\"stats/vm_objects\">VM_Objects\n");
    printf("<OPTION VALUE=\"server_list\">Cache Server List\n");
    printf("<OPTION VALUE=\"stats/ipcache\">IP Cache Contents\n");
    printf("<OPTION VALUE=\"stats/fqdncache\">FQDN Cache Contents\n");
    printf("<OPTION VALUE=\"stats/dns\">DNS Server Statistics\n");
    printf("<OPTION VALUE=\"stats/redirector\">Redirector Statistics\n");
    printf("<OPTION VALUE=\"shutdown\">Shutdown Cache (password required)\n");
    printf("<OPTION VALUE=\"refresh\">Refresh Object (URL required)\n");
#ifdef REMOVE_OBJECT
    printf("<OPTION VALUE=\"remove\">Remove Object (URL required)\n");
#endif
    printf("</SELECT>\n\n");
    printf("<HR>\n");
    printf("<INPUT TYPE=\"submit\"> <INPUT TYPE=\"reset\">\n");
    printf("</FORM></PRE>\n");
    print_trailer();
}

/* A utility function from the NCSA httpd cgi-src utils.c */
char *
makeword(char *line, char stop)
{
    int x = 0, y;
    char *word = xmalloc(sizeof(char) * (strlen(line) + 1));

    for (x = 0; ((line[x]) && (line[x] != stop)); x++)
	word[x] = line[x];

    word[x] = '\0';
    if (line[x])
	++x;
    y = 0;

    while ((line[y++] = line[x++]) != '\0');
    return word;
}

/* A utility function from the NCSA httpd cgi-src utils.c */
char *
fmakeword(FILE * f, char stop, int *cl)
{
    int wsize = 102400;
    char *word = NULL;
    int ll = 0;

    word = xmalloc(sizeof(char) * (wsize + 1));
    for (;;) {
	word[ll] = (char) fgetc(f);
	if (ll == wsize) {
	    word[ll + 1] = '\0';
	    wsize += 102400;
	    word = realloc(word, sizeof(char) * (wsize + 1));
	}
	--(*cl);
	if ((word[ll] == stop) || (feof(f)) || (!(*cl))) {
	    if (word[ll] != stop)
		ll++;
	    word[ll] = '\0';
	    return word;
	}
	++ll;
    }
    /* NOTREACHED */
}

/* A utility function from the NCSA httpd cgi-src utils.c */
char
x2c(char *what)
{
    char digit;

    digit = (what[0] >= 'A' ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
    return (digit);
}

/* A utility function from the NCSA httpd cgi-src utils.c */
void
unescape_url(char *url)
{
    int x, y;

    for (x = 0, y = 0; url[y]; ++x, ++y) {
	if ((url[x] = url[y]) == '%') {
	    url[x] = x2c(&url[y + 1]);
	    y += 2;
	}
    }
    url[x] = '\0';
}

/* A utility function from the NCSA httpd cgi-src utils.c */
void
plustospace(char *str)
{
    int x;

    for (x = 0; str[x]; x++)
	if (str[x] == '+')
	    str[x] = ' ';
}


void
parse_object(char *string)
{
    char *tmp_line = NULL;
    char *url = NULL;
    char *token = NULL;
    char *store_time = NULL;
    char *last_ref = NULL;
    char *ttl = NULL;
    char *sto = NULL;
    char *status = NULL;
    char *obj_status = NULL;
    char *w_space = " \t\n";
    int obj_size;
    int ref_cnt;

    /* Use tmp_line as a temporary pointer to the input line */
    tmp_line = string;

    /* Parse out the url */
    url = strtok(tmp_line, w_space);

    if (!url)
	return;

#if !ALL_OBJECTS
    if (!strncmp(url, "cache_object", 12))
	return;
    if (!strncmp(url, "POST", 4))
	return;
#endif

    tmp_line = NULL;

    token = strtok(tmp_line, w_space);
    sscanf(token, "%d", &obj_size);

    token = strtok(tmp_line, w_space);
    store_time = strdup(token);

    token = strtok(tmp_line, w_space);
    obj_status = strdup(token);

    token = strtok(tmp_line, w_space);
    last_ref = strdup(token);

    token = strtok(tmp_line, w_space);
    ttl = strdup(token);

    token = strtok(tmp_line, w_space);
    /* Active */

    token = strtok(tmp_line, w_space);
    sscanf(token, "%d", &ref_cnt);

    token = strtok(tmp_line, w_space);
    sto = strdup(token);

    token = strtok(tmp_line, w_space);
    status = strdup(token);

    printf("<LI>Cache: <A HREF=\"%s\">%s</A><BR>",
	url, url);
    printf("Size: %d bytes, TTL: %s ,<BR>",
	obj_size, ttl);
    printf("Stored: %s, %s ago, %s %s,<BR>",
	sto, store_time, obj_status, status);
    printf("Refs: %d, Referenced %s hh:mm:ss ago</LI>\n",
	ref_cnt, last_ref);

    free(ttl);
    free(store_time);
    free(last_ref);
    free(sto);
    free(status);
}

int
main(int argc, char *argv[])
{
    static char hostname[256];
    static char operation[256];
    static char password[256];
    static char url[4096];
    static char msg[1024];
    static char buf[4096];
    static char reserve[4096];
    static char s1[255];
    static char s2[255];
    char *time_string = NULL;
    char *agent = NULL;
    char *s = NULL;
    char *qs = NULL;
    char query_host[256];
    int query_port;
    int got_data = 0;
    int x;
    int cl;
    int conn;
    int len;
    int bytesWritten;
    int portnum = CACHE_HTTP_PORT;
    int op = 0;
    int p_state;
    int n_loops;
    int cpy_ind;
    int indx;
    int in_list = 0;
    int in_table = 0;
    int d1, d2, d3, d4, d5, d6, d7;
    int single = TRUE;
    float f1;
    time_t time_val;
    entry entries[MAX_ENTRIES];

    if ((s = strrchr(argv[0], '/')))
	progname = strdup(s + 1);
    else
	progname = strdup(argv[0]);

    if ((qs = getenv("QUERY_STRING")) != NULL) {
	s = strchr(qs, ':');	/* A colon separates the port from the host */
	if (s != NULL)
	    query_port = atoi(s + 1);	/* port */
	else {
	    query_port = CACHE_HTTP_PORT;	/* Assume the default */
	    s = qs + strlen(qs);
	}
	strncpy(query_host, qs, (s - qs));	/* host */
	query_host[s - qs] = '\0';
    } else {			/* Use the defaults */
	strcpy(query_host, CACHEMGR_HOSTNAME);
	query_port = CACHE_HTTP_PORT;
    }

    if ((s = getenv("SCRIPT_NAME")) != NULL) {
	script_name = strdup(s);
    }
    printf("Content-type: text/html\r\n\r\n");
    if ((agent = getenv("HTTP_USER_AGENT")) != NULL) {
	if (!strncasecmp(agent, "Mozilla", 7) ||
	    !strncasecmp(agent, "OmniWeb/2", 9) ||
	    !strncasecmp(agent, "Netscape", 8)) {
	    hasTables = TRUE;
	}
    }
    hostname[0] = '\0';
    if ((s = getenv("CONTENT_LENGTH")) == NULL) {
	noargs_html(query_host, query_port);
	exit(0);
    }
    cl = atoi(s);
    password[0] = url[0] = '\0';
    for (x = 0; cl && (!feof(stdin)); x++) {
	got_data = 1;
	entries[x].val = fmakeword(stdin, '&', &cl);
	plustospace(entries[x].val);
	unescape_url(entries[x].val);
	entries[x].name = makeword(entries[x].val, '=');
	if (!strncmp(entries[x].name, "host", 4))
	    strncpy(hostname, entries[x].val, 256);
	else if (!strncmp(entries[x].name, "operation", 7))
	    strncpy(operation, entries[x].val, 256);
	else if (!strncmp(entries[x].name, "password", 8))
	    strncpy(password, entries[x].val, 256);
	else if (!strncmp(entries[x].name, "url", 3))
	    strncpy(url, entries[x].val, 4096);
	else if (!strncmp(entries[x].name, "port", 4))
	    portnum = atoi(entries[x].val);
	else {
	    printf("<P><STRONG>Unknown CGI parameter: %s</STRONG></P>\n",
		entries[x].name);
	    noargs_html(query_host, query_port);
	    exit(0);
	}
    }
    if (!got_data) {		/* prints HTML form if no args */
	noargs_html(query_host, query_port);
	exit(0);
    }
    if (hostname[0] == '\0') {
	printf("<H1>ERROR</H1>\n");
	printf("<P><STRONG>You must provide a hostname!\n</STRONG></P><HR>");
	noargs_html(query_host, query_port);
	exit(0);
    }
    close(0);

    if (!strcmp(operation, "info") ||
	!strcmp(operation, "Cache Information")) {
	op = INFO;
    } else if (!strcmp(operation, "squid.conf") ||
	!strcmp(operation, "Cache Configuration File")) {
	op = CACHED;
    } else if (!strcmp(operation, "server_list") ||
	!strcmp(operation, "Cache Server List")) {
	op = SERVER;
#ifdef MENU_SHOW_LOG
    } else if (!strcmp(operation, "log") ||
	!strcmp(operation, "Cache Log")) {
	op = LOG;
#endif
    } else if (!strcmp(operation, "parameter") ||
	!strcmp(operation, "Cache Parameters")) {
	op = PARAM;
    } else if (!strcmp(operation, "stats/ipcache") ||
	!strcmp(operation, "IP Cache")) {
	op = STATS_I;
    } else if (!strcmp(operation, "stats/fqdncache") ||
	!strcmp(operation, "FQDN Cache")) {
	op = STATS_F;
    } else if (!strcmp(operation, "stats/dns") ||
	!strcmp(operation, "DNS Server Stats")) {
	op = STATS_D;
    } else if (!strcmp(operation, "stats/redirector") ||
	!strcmp(operation, "Redirection Server Stats")) {
	op = STATS_R;
    } else if (!strcmp(operation, "stats/vm_objects") ||
	!strcmp(operation, "VM_Objects")) {
	op = STATS_VM;
    } else if (!strcmp(operation, "stats/objects") ||
	!strcmp(operation, "Objects")) {
	op = STATS_O;
    } else if (!strcmp(operation, "stats/utilization") ||
	!strcmp(operation, "Utilization")) {
	op = STATS_U;
    } else if (!strcmp(operation, "stats/io") ||
	!strcmp(operation, "I/O")) {
	op = STATS_IO;
    } else if (!strcmp(operation, "stats/reply_headers") ||
	!strcmp(operation, "Reply Headers")) {
	op = STATS_HDRS;
    } else if (!strcmp(operation, "stats/filedescriptors") ||
	!strcmp(operation, "Filedescriptor")) {
	op = STATS_FDS;
    } else if (!strcmp(operation, "shutdown")) {
	op = SHUTDOWN;
    } else if (!strcmp(operation, "refresh")) {
	op = REFRESH;
#ifdef REMOVE_OBJECT
    } else if (!strcmp(operation, "remove")) {
	op = REMOVE;
#endif
    } else {
	printf("Unknown operation: %s\n", operation);
	exit(0);
    }

    switch (op) {
    case INFO:
    case CACHED:
    case SERVER:
    case LOG:
    case PARAM:
    case STATS_I:
    case STATS_F:
    case STATS_D:
    case STATS_R:
    case STATS_O:
    case STATS_VM:
    case STATS_U:
    case STATS_IO:
    case STATS_HDRS:
    case STATS_FDS:
	sprintf(msg, "GET cache_object://%s/%s HTTP/1.0\r\n\r\n",
	    hostname, op_cmds[op]);
	break;
    case SHUTDOWN:
	sprintf(msg, "GET cache_object://%s/%s@%s HTTP/1.0\r\n\r\n",
	    hostname, op_cmds[op], password);
	break;
    case REFRESH:
	sprintf(msg, "GET %s HTTP/1.0\r\nPragma: no-cache\r\nAccept: */*\r\n\r\n", url);
	break;
#ifdef REMOVE_OBJECT
    case REMOVE:
	printf("Remove not yet supported\n");
	exit(0);
	/* NOTREACHED */
#endif
    default:
    case MAXOP:
	printf("Unknown operation: %s\n", operation);
	exit(0);
	/* NOTREACHED */
    }

    time_val = time(NULL);
    time_string = ctime(&time_val);

    printf("<HTML><HEAD><TITLE>Cache Manager: %s:%s:%d</TITLE></HEAD>\n",
	operation, hostname, portnum);
    printf("<BODY><FORM METHOD=\"POST\" ACTION=\"%s?%s:%d\">\n",
	script_name, query_host, query_port);
    printf("<INPUT TYPE=\"submit\" VALUE=\"Refresh\">\n");
    printf("<SELECT NAME=\"operation\">\n");
    printf("<OPTION SELECTED VALUE=\"%s\">Current\n", operation);
    printf("<OPTION VALUE=\"info\">Cache Information\n");
    printf("<OPTION VALUE=\"squid.conf\">Cache Configuration File\n");
    printf("<OPTION VALUE=\"parameter\">Cache Parameters\n");
#ifdef MENU_SHOW_LOG
    printf("<OPTION VALUE=\"log\">Cache Log\n");
#endif
    printf("<OPTION VALUE=\"stats/utilization\">Utilization\n");
    printf("<OPTION VALUE=\"stats/io\">I/O\n");
    printf("<OPTION VALUE=\"stats/reply_headers\">HTTP Reply Headers\n");
    printf("<OPTION VALUE=\"stats/filedescriptors\">Filedescriptor Usage\n");
    printf("<OPTION VALUE=\"stats/objects\">Objects\n");
    printf("<OPTION VALUE=\"stats/vm_objects\">VM_Objects\n");
    printf("<OPTION VALUE=\"server_list\">Cache Server List\n");
    printf("<OPTION VALUE=\"stats/ipcache\">IP Cache Contents\n");
    printf("<OPTION VALUE=\"stats/fqdncache\">FQDN Cache Contents\n");
    printf("<OPTION VALUE=\"stats/dns\">DNS Server Statistics\n");
    printf("<OPTION VALUE=\"stats/redirector\">Redirector Statistics\n");
    printf("</SELECT>");
    printf("<INPUT TYPE=\"hidden\" NAME=\"host\" VALUE=\"%s\">\n", hostname);
    printf("<INPUT TYPE=\"hidden\" NAME=\"port\" VALUE=\"%d\">\n", portnum);
    printf("<INPUT TYPE=\"hidden\" NAME=\"password\" VALUE=\"NOT_PERMITTED\">\n");
    printf("</FORM>");
    printf("<p><em><A HREF=\"%s?%s:%d\">Empty form</A></em></p>\n", script_name,
	query_host, query_port);
    printf("<HR>\n");

    printf("<H1>%s:  %s:%d</H1>\n", operation,
	hostname, portnum);
    printf("<p>dated %s</p>\n", time_string);
    printf("<PRE>\n");

    /* Connect to the server */
    if ((conn = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
	perror("client: socket");
	exit(1);
    }
    if ((conn = client_comm_connect(conn, hostname, portnum)) < 0) {
	printf("Error: connecting to cache mgr: %s:%d\n", hostname, portnum);
	printf("%s</PRE></BODY></HTML>\n", xstrerror());
	exit(1);
    }
    bytesWritten = write(conn, msg, strlen(msg));

    if (bytesWritten < 0) {
	printf("Error: write failed\n");
	exit(1);
    } else if (bytesWritten != (strlen(msg))) {
	printf("Error: write short\n");
	exit(1);
    }
    /* Print header stuff for tables */
    switch (op) {
    case INFO:
    case CACHED:
    case SERVER:
    case LOG:
    case STATS_I:
    case STATS_F:
    case STATS_D:
    case STATS_R:
    case STATS_O:
    case STATS_VM:
    case STATS_IO:
    case STATS_HDRS:
    case STATS_FDS:
    case SHUTDOWN:
    case REFRESH:
	break;
    case PARAM:
	if (hasTables) {
	    printf("<table border=1><tr><td><STRONG>Parameter</STRONG><td><STRONG>Value</STRONG><td><STRONG>Description</STRONG>\n");
	    in_table = 1;
	} else {
	    printf("\n    Parameter   Value   Description\n");
	    printf("-------------- ------- -------------------------------------\n");
	}
	break;
    case STATS_U:
	if (hasTables) {
	    printf("<table border=1><tr><td><STRONG>Protocol</STRONG><td><STRONG>Count</STRONG><td><STRONG>Max KB</STRONG><td><STRONG>Current KB</STRONG><td><STRONG>Min KB</STRONG><td><STRONG>Hit Ratio</STRONG><td><STRONG>Transfer Rate</STRONG><td><STRONG>References</STRONG><td><STRONG>Transfered KB</STRONG>\n");
	    in_table = 1;
	} else {
	    printf("Protocol  Count   Maximum   Current   Minimum  Hit  Trans   Transfer Transfered\n");
	    printf("                  KB        KB        KB       Rate KB/sec  Count     KB\n");
	    printf("-------- ------- --------- --------- --------- ---- ------ --------- ----------\n");
	}
	break;
    default:
	printf("\n\n<P>\nNot currently implemented.\n");
	exit(1);
    }

    p_state = 0;
    cpy_ind = 0;
    n_loops = 0;		/* Keep track of the number of passes through while */
    while ((len = read(conn, buf, sizeof(buf))) > 0) {
	n_loops++;
	/* Simple state machine for parsing a {{ } { } ...} style list */
	for (indx = 0; indx < len; indx++) {
	    if (buf[indx] == '{')
		p_state++;
	    else if (buf[indx] == '}')
		if (p_state == 2) {	/* Have an element of the list */
		    single = FALSE;
		    p_state++;
		    reserve[cpy_ind] = '\0';
		    cpy_ind = 0;
		} else if (p_state == 1 && single)	/* Check for single element list */
		    p_state = 3;
		else		/* End of list */
		    p_state = 0;
	    else if ((indx == 0) && (n_loops == 1)) {
		if (op != REFRESH)
		    printf("ERROR:%s\n", buf);	/* Must be an error message, pass it on */
		else
		    printf("Refreshed URL: %s\n", url);
	    } else
		reserve[cpy_ind++] = buf[indx];


	    /* Have an element of the list, so parse reserve[] accordingly */
	    if (p_state == 3) {
		int sn;
		switch (op) {
		case INFO:
		case CACHED:
		case SERVER:
		case LOG:
		case STATS_I:
		case STATS_F:
		case STATS_D:
		case STATS_R:
		case STATS_IO:
		case STATS_HDRS:
		case STATS_FDS:
		case SHUTDOWN:
		    p_state = 1;
		    printf("%s", reserve);
		    break;
		case REFRESH:
		    /* throw object away */
		    break;
		case PARAM:
		    p_state = 1;
		    memset(s1, '\0', 255);
		    memset(s2, '\0', 255);
		    d1 = 0;
		    sscanf(reserve, "%s %d \"%[^\"]", s1, &d1, s2);
		    if (hasTables)
			printf("<tr><td><STRONG>%s</STRONG><td ALIGN=\"right\">%d<td>%s\n", s1, d1, s2 + 2);
		    else
			printf("%14s %7d %s\n", s1, d1, s2 + 2);
		    break;
		case STATS_U:
		    p_state = 1;
		    sn = sscanf(reserve, "%s %d %d %d %d %f %d %d %d",
			s1, &d1, &d2, &d3, &d4, &f1, &d5, &d6, &d7);
		    if (sn == 1) {
			if (hasTables)
			    printf("<tr><td align=\"right\"><B>%s</B>", s1);
			else
			    printf("%8s", s1);
			break;
		    }
		    if (hasTables)
			printf("<tr><td align=\"right\"><B>%s</B><td align=\"right\">%d<td align=\"right\">%d<td align=\"right\">%d<td align=\"right\">%d<td align=\"right\">%4.2f<td align=\"right\">%d<td align=\"right\">%d<td align=\"right\">%d",
			    s1, d1, d2, d3, d4, f1, d5, d6, d7);
		    else
			printf("%8s %7d %9d %9d %9d %4.2f %6d %9d %10d\n",
			    s1, d1, d2, d3, d4, f1, d5, d6, d7);
		    break;
		case STATS_O:
		case STATS_VM:
		    if (!in_list) {
			in_list = 1;
			printf("<OL>\n");
		    }
		    parse_object(reserve);
		    p_state = 1;
		    break;
		default:
		    printf("%s\n", "Not currently implemented");
		    exit(1);
		}
	    }
	}
    }

    if (in_list)
	printf("</OL>\n");

    if (in_table)
	printf("</table>\n");

    printf("\n</PRE>\n");
    print_trailer();
    (void) close(conn);
    exit(0);
    /* NOTREACHED */
    return 0;
}

static int
client_comm_connect(int sock, char *dest_host, u_short dest_port)
{
    struct hostent *hp;
    static struct sockaddr_in to_addr;

    /* Set up the destination socket address for message to send to. */
    to_addr.sin_family = AF_INET;

    if ((hp = gethostbyname(dest_host)) == 0) {
	return (-1);
    }
    xmemcpy(&to_addr.sin_addr, hp->h_addr, hp->h_length);
    to_addr.sin_port = htons(dest_port);
    return connect(sock, (struct sockaddr *) &to_addr, sizeof(struct sockaddr_in));
}

/* $Id$ */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

/* Local functions */
static int client_comm_connect();
static void usage();

static void usage(progname)
     char *progname;
{
    fprintf(stderr, "\
Usage: %s [-rs] [-h host] [-p port] url\n\
Options:\n\
    -r         Force cache to reload URL.\n\
    -s         Silent.  Do not print data to stdout.\n\
    -h host    Retrieve URL from cache on hostname.  Default is localhost.\n\
    -p port    Port number of cache.  Default is %d.\n\
", progname, CACHE_HTTP_PORT);
    exit(1);
}

int main(argc, argv)
     int argc;
     char *argv[];
{
    int conn, c, len, bytesWritten;
    int port, to_stdout, reload;
    char url[BUFSIZ], msg[BUFSIZ], buf[BUFSIZ], hostname[BUFSIZ];
    extern char *optarg;

    /* set the defaults */
    strcpy(hostname, "localhost");
    port = CACHE_HTTP_PORT;
    to_stdout = 1;
    reload = 0;

    if (argc < 2) {
	usage(argv[0]);		/* need URL */
    } else if (argc >= 2) {
	strcpy(url, argv[argc - 1]);
	if (url[0] == '-')
	    usage(argv[0]);
	while ((c = getopt(argc, argv, "fsrnp:c:h:?")) != -1)
	    switch (c) {
	    case 'h':		/* host:arg */
	    case 'c':		/* backward compat */
		if (optarg != NULL)
		    strcpy(hostname, optarg);
		break;
	    case 's':		/* silent */
	    case 'n':		/* backward compat */
		to_stdout = 0;
		break;
	    case 'r':		/* reload */
		reload = 1;
		break;
	    case 'p':		/* port number */
		sscanf(optarg, "%d", &port);
		if (port < 1)
		    port = CACHE_HTTP_PORT;	/* default */
		break;
	    case '?':		/* usage */
	    default:
		usage(argv[0]);
		break;
	    }
    }
    /* Connect to the server */
    if ((conn = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
	perror("client: socket");
	exit(1);
    }
    if (client_comm_connect(conn, hostname, port) < 0) {
	if (errno == 0) {
	    fprintf(stderr, "client: ERROR: Cannot connect to %s:%d: Host unknown.\n", hostname, port);
	} else {
	    char tbuf[BUFSIZ];
	    sprintf(tbuf, "client: ERROR: Cannot connect to %s:%d",
		hostname, port);
	    perror(tbuf);
	}
	exit(1);
    }
    /* Build the HTTP request */
    if (reload) {
	sprintf(msg, "GET %s HTTP/1.0\r\nPragma: no-cache\r\nAccept: */*\r\n\r\n", url);
    } else {
	sprintf(msg, "GET %s HTTP/1.0\r\nAccept: */*\r\n\r\n", url);
    }

    /* Send the HTTP request */
    bytesWritten = write(conn, msg, strlen(msg));
    if (bytesWritten < 0) {
	perror("client: ERROR: write");
	exit(1);
    } else if (bytesWritten != strlen(msg)) {
	fprintf(stderr, "client: ERROR: Cannot send request?: %s\n", msg);
	exit(1);
    }
    /* Read the data */
    while ((len = read(conn, buf, sizeof(buf))) > 0) {
	if (to_stdout)
	    fwrite(buf, len, 1, stdout);
    }
    (void) close(conn);		/* done with socket */
    exit(0);
    /*NOTREACHED */
}

static int client_comm_connect(sock, dest_host, dest_port)
     int sock;			/* Type of communication to use. */
     char *dest_host;		/* Server's host name. */
     int dest_port;		/* Server's port. */
{
    struct hostent *hp;
    static struct sockaddr_in to_addr;

    /* Set up the destination socket address for message to send to. */
    to_addr.sin_family = AF_INET;

    if ((hp = gethostbyname(dest_host)) == 0) {
	return (-1);
    }
    memcpy(&to_addr.sin_addr, hp->h_addr, hp->h_length);
    to_addr.sin_port = htons(dest_port);
    return connect(sock, (struct sockaddr *) &to_addr, sizeof(struct sockaddr_in));
}

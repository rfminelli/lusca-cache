#include "config.h"

/*
 * On some systems, FD_SETSIZE is set to something lower than the
 * actual number of files which can be opened.  IRIX is one case,
 * NetBSD is another.  So here we increase FD_SETSIZE to our
 * configure-discovered maximum *before* any system includes.
 */
#define CHANGE_FD_SETSIZE 1

/* Cannot increase FD_SETSIZE on Linux */
#if defined(_SQUID_LINUX_)
#undef CHANGE_FD_SETSIZE
#define CHANGE_FD_SETSIZE 0
#endif

/* Cannot increase FD_SETSIZE on FreeBSD before 2.2.0, causes select(2)
 * to return EINVAL. */
/* Marian Durkovic <marian@svf.stuba.sk> */
/* Peter Wemm <peter@spinner.DIALix.COM> */
#if defined(_SQUID_FREEBSD_)
#include <osreldate.h>
#if __FreeBSD_version < 220000
#undef CHANGE_FD_SETSIZE
#define CHANGE_FD_SETSIZE 0
#endif
#endif

/* Increase FD_SETSIZE if SQUID_MAXFD is bigger */
#if CHANGE_FD_SETSIZE && SQUID_MAXFD > DEFAULT_FD_SETSIZE
#define FD_SETSIZE SQUID_MAXFD
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#if HAVE_TIME_H
#include <time.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
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
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_ASSERT_H
#include <assert.h>
#endif

#define PROXY_PORT 3128
#define PROXY_ADDR "127.0.0.1"
#define MAX_FDS 1024
#define READ_BUF_SZ 4096

static int proxy_port = PROXY_PORT;
static char *proxy_addr = PROXY_ADDR;
static char *progname;
static int reqpersec;
static int nrequests;
static int opt_ims = 0;
static int opt_range = 0;
static int max_connections = 64;
static time_t lifetime = 60;
static struct timeval now;
static long total_bytes_written = 0;
static long total_bytes_read = 0;
static int opt_checksum = 0;

typedef void (CB) (int, void *);

struct _f {
    CB *cb;
    CB *ccb;
    void *data;
    time_t start;
};
struct _request {
    int fd;
    char url[8192];
    char buf[READ_BUF_SZ * 2 + 1];
    int headfound;
    long validsize;
    long validsum;
    long bodysize;
    long sum;
    int content_length;
};

struct _f FD[MAX_FDS];
int nfds = 0;
int maxfd = 0;


char *
mkrfc850(t)
     time_t *t;
{
    static char buf[128];
    struct tm *gmt = gmtime(t);
    buf[0] = '\0';
    (void) strftime(buf, 127, "%A, %d-%b-%y %H:%M:%S GMT", gmt);
    return buf;
}

void
fd_close(int fd)
{
    close(fd);
    if (FD[fd].ccb)
	FD[fd].ccb(fd, FD[fd].data);
    FD[fd].ccb = NULL;
    FD[fd].cb = NULL;
    FD[fd].data = NULL;
    nfds--;
    if (fd == maxfd) {
	while (fd > 0 && FD[fd].cb == NULL)
	    fd--;
	maxfd = fd;
    }
}

void
fd_open(int fd, CB * cb, void *data, CB *ccb)
{
    FD[fd].cb = cb;
    FD[fd].ccb = ccb;
    FD[fd].data = data;
    FD[fd].start = now.tv_sec;
    if (fd > maxfd)
	maxfd = fd;
    nfds++;
}

void
sig_intr(int sig)
{
    fd_close(0);
    nfds++;
    printf("\rWaiting for open connections to finish...\n");
    signal(sig, SIG_DFL);
}

void
read_reply(int fd, void *data)
{
    struct _request *r = data;
    static unsigned char buf[READ_BUF_SZ];
    int len;
    if ((len=read(fd, buf, READ_BUF_SZ)) <= 0) {
	fd_close(fd);
	reqpersec++;
	nrequests++;
    } else {
	int used=0;
	total_bytes_read+=len;
	if (r->headfound < 2) {
	    char *p,*header = NULL;
	    int oldlen = strlen(r->buf);
	    int newlen = oldlen + len;
	    assert(oldlen <= READ_BUF_SZ);
	    memcpy(r->buf+oldlen, buf, len);
	    r->buf[newlen+1]='\0';
	    for(p=r->buf; r->headfound < 2 && used<newlen; p++,used++) {
		switch(*p) {
		case '\n':
		    r->headfound++;
		    if (header) {
			/* Decode header */
			if (strncasecmp(header,"Content-Length:",15)==0)
			    r->content_length = atoi(header+15);
			if (strncasecmp(header,"X-Request-URI:",14)==0) {
			    /* Check URI */
			    if (strncmp(r->url, header+15, strcspn(header+15,"\r\n"))) {
				char url[8192];
				strncpy(url, header+15, strcspn(header+15,"\r\n"));
				url[strcspn(header+15, "\r\n")]='\n';
				fprintf(stderr,"ERROR: Sent %s received %s\n",
					r->url, url);
			    }
			}
			header=NULL;
		    }
		    break;
		case '\r':
		    break;
		default:
		    r->headfound=0;
		    if (!header)
			header = p;
		    break;
		}
	    }
	    if (header) {
		memmove(r->buf, header, newlen - (header - r->buf) + 1);
	    }
	}
	r->bodysize+=len-used;
	if (opt_checksum) {
	    for (; used<len ; used++) {
		r->sum += buf[used];
	    }
	}
    }
}

void
reply_done(int fd, void *data)
{
    struct _request *r = data;
    if (opt_range)
	; /* skip size checks for now */
    else
    if (r->bodysize != r->content_length)
	fprintf(stderr,"ERROR: %s expected %d bytes got %d\n",
		r->url, r->content_length, r->bodysize);
    else if (r->validsize >= 0) {
	if (r->validsize != r->bodysize)
	    fprintf(stderr,"WARNING: %s size mismatch wanted %d bytes got %d\n",
		    r->url, r->validsize, r->bodysize);
	else if (opt_checksum && r->validsum != r->sum)
	    fprintf(stderr,"WARNING: %s invalid checksum wanted %d got %d\n",
		    r->url, r->validsum, r->sum);
    } else if (opt_checksum) {
	fprintf(stderr,"DONE: %s checksum %d size %d\n",
		r->url, r->sum, r->bodysize);
    }
    free(r);
}

struct _request *
request(char *urlin)
{
    int s=-1,f=-1;
    char buf[4096];
    char msg[8192];
    char *method, *url, *file, *size, *checksum;
    char urlbuf[8192];
    int len,len2;
    time_t w;
    struct stat st;
    struct sockaddr_in S;
    struct _request *r;
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	return NULL;
    }
    memset(&S, '\0', sizeof(struct sockaddr_in));
    S.sin_family = AF_INET;
    S.sin_port = htons(proxy_port);
    S.sin_addr.s_addr = inet_addr(proxy_addr);
    if (connect(s, (struct sockaddr *) &S, sizeof(S)) < 0) {
	close(s);
	perror("connect");
	return NULL;
    }
    strcpy(urlbuf,urlin);
    method=strtok(urlbuf," ");
    url=strtok(NULL," ");
    file=strtok(NULL," ");
    size=strtok(NULL," ");
    checksum=strtok(NULL," ");
    if (!url) {
	url=method;
	method="GET";
    }
    r=calloc(1,sizeof *r);
    assert(r!=NULL);
    strcpy(r->url, url);
    r->fd = s;
    if (size && strcmp(size,"-")!=0)
	r->validsize=atoi(size);
    else
	r->validsize=-1; /* Unknown */
    if (checksum && strcmp(checksum,"-")!=0)
	r->validsum=atoi(checksum);
    msg[0] = '\0';
    sprintf(buf,"%s %s HTTP/1.0\r\n", method, url);
    strcat(msg, buf);
    strcat(msg, "Accept: */*\r\n");
    if (opt_ims && (lrand48() & 0x03) == 0) {
	w = time(NULL) - (lrand48() & 0x3FFFF);
	sprintf(buf, "If-Modified-Since: %s\r\n", mkrfc850(&w));
	strcat(msg,buf);
    }
    if (file && strcmp(file, "-")!=0) {
	f = open(file,O_RDONLY);
	if (f < 0) {
	    perror("open file");
	    exit(1);
	}
	fstat(f, &st);
	sprintf(buf,"Content-Length: %d\r\n", st.st_size);
	strcat(msg,buf);
    }
    if (opt_range && (lrand48() & 0x03) == 0) {
	int len;
	int count = 0;
	strcat(msg, "Range: bytes=");
	while (((len = (int)lrand48()) & 0x03) == 0 || !count) {
	    const int offset = (int) lrand48();
	    if (count)
		strcat(msg, ",");
	    switch (lrand48() & 0x03) {
		case 0:
		    sprintf(buf, "-%d", len);
		    break;
		case 1:
		    sprintf(buf, "%d-", offset);
		    break;
		default:
		    sprintf(buf, "%d-%d", offset, offset+len);
		    break;
	    }
	    strcat(msg,buf);
	    count++;
	}
	strcat(msg,"\r\n");
    }
    strcat(msg, "\r\n");
    len = strlen(msg);
    if ((len2=write(s, msg, len)) != len) {
	close(s);
	perror("write request");
	free(r);
	return NULL;
    } else
	total_bytes_written += len2;
    if (f>=0) {
	while ((len = read(f, buf, sizeof(buf)))>0) {
	    len2 = write(s, buf, len);
	    if (len2 < 0) {
		perror("write body");
		close(s);
		free(r);
	    }
	}
	if (len < 0) {
	    perror("read body");
	    exit(1);
	}
    }

/*
 * if (fcntl(s, F_SETFL, O_NDELAY) < 0)
 * perror("fcntl O_NDELAY");
 */
    return r;
}

void
read_url(int fd, void *junk)
{
    struct _request *r;
    static char buf[8192];
    char *t;
    if (fgets(buf, 8191, stdin) == NULL) {
	printf("Done Reading URLS\n");
	fd_close(0);
	nfds++;
	return;
    }
    if ((t = strchr(buf, '\n')))
	*t = '\0';
    r = request(buf);
    if (!r) {
	max_connections = nfds - 1;
	printf("NOTE: max_connections set at %d\n", max_connections);
    } else {
	fd_open(r->fd, read_reply, r, reply_done);
    }
}

void
usage(void)
{
    fprintf(stderr, "usage: %s: [-cir] -p port -h host -n max\n", progname);
}

int
main(argc, argv)
     int argc;
     char *argv[];
{
    int i;
    int c;
    int dt;
    int j;
    fd_set R,R2;
    struct timeval start;
    struct timeval last;
    struct timeval to;
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    progname = strdup(argv[0]);
    gettimeofday(&now, NULL);
    start = last = now;
    while ((c = getopt(argc, argv, "p:h:n:icrl:")) != -1) {
	switch (c) {
	case 'p':
	    proxy_port = atoi(optarg);
	    break;
	case 'h':
	    proxy_addr = strdup(optarg);
	    break;
	case 'n':
	    max_connections = atoi(optarg);
	    break;
	case 'i':
	    opt_ims = 1;
	    break;
	case 'l':
	    lifetime = (time_t) atoi(optarg);
	    break;
	case 'c':
	    opt_checksum = 1;
	    break;
	case 'r':
	    opt_range = 1;
	    break;
	default:
	    usage();
	    return 1;
	}
    }
    fd_open(0, read_url, NULL, NULL);
    nfds--;
    signal(SIGINT, sig_intr);
    signal(SIGPIPE, SIG_IGN);
    FD_ZERO(&R2);
    while (nfds || FD[0].cb) {
	FD_ZERO(&R);
	to.tv_sec = 0;
	to.tv_usec = 100000;
	if (nfds < max_connections && FD[0].cb)
	    FD_SET(0, &R);
	for (i = 1; i <= maxfd; i++) {
	    if (FD[i].cb == NULL)
		continue;
	    if (now.tv_sec - FD[i].start > lifetime) {
		fprintf(stderr, "WARNING: fd %d lifetime timeout\n", i);
		fd_close(i);
		continue;
	    }
	    FD_SET(i, &R);
	}
	if (select(maxfd + 1, &R, NULL, NULL, &to) < 0) {
	    fprintf(stderr, "maxfd=%d\n", maxfd);
	    if (errno != EINTR)
		perror("select");
	    continue;
	}
	gettimeofday(&now, NULL);
	for (i = 0; i <= maxfd; i++) {
	    if (!FD_ISSET(i, &R))
		continue;
	    FD[i].cb(i, FD[i].data);
            if (nfds < max_connections && FD[0].cb) {
		j=0;
		FD_SET(0,&R2);
		to.tv_sec=0;
		to.tv_usec=0;
		if(select(1,&R2,NULL,NULL,&to) == 1)
		    FD[0].cb(0, FD[0].data);
	    }
	}
	if (now.tv_sec > last.tv_sec) {
	    last = now;
	    dt = (int) (now.tv_sec - start.tv_sec);
	    printf("T+ %6d: %9d req (%+4d), %4d conn, %3d/sec avg, %dmb, %dkb/sec avg\n",
		dt,
		nrequests,
		reqpersec,
		nfds,
		(int) (nrequests / dt),
		(int)total_bytes_read / 1024 / 1024,
		(int)total_bytes_read / 1024 / dt);
	    reqpersec = 0;
	}
    }
    return 0;
}

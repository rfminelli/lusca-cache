#include "config.h"

#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_ASSERT_H
#include <assert.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SIGNAL_H
#include <signal.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_PATHS_H
#include <paths.h>
#endif

#include "defines.h"

#define SQUID_MAXPATHLEN 256
#ifndef MAXPATHLEN
#define MAXPATHLEN SQUID_MAXPATHLEN
#endif

/* parse buffer - ie, length of longest expected line */
#define	LOGFILE_BUF_LEN		65536

int do_flush = 0;

static void
signal_alarm(int unused)
{
    do_flush = 1;
}

static void
rotate(const char *path, int rotate_count)
{
#ifdef S_ISREG
    struct stat sb;
#endif
    int i;
    char from[MAXPATHLEN];
    char to[MAXPATHLEN];
    assert(path);
#ifdef S_ISREG
    if (stat(path, &sb) == 0)
	if (S_ISREG(sb.st_mode) == 0)
	    return;
#endif
    /* Rotate numbers 0 through N up one */
    for (i = rotate_count; i > 1;) {
	i--;
	snprintf(from, MAXPATHLEN, "%s.%d", path, i - 1);
	snprintf(to, MAXPATHLEN, "%s.%d", path, i);
	rename(from, to);
    }
    if (rotate_count > 0) {
	snprintf(to, MAXPATHLEN, "%s.%d", path, 0);
	rename(path, to);
    }
}

/*
 * The commands:
 *
 * L<data>\n - logfile data
 * R\n - rotate file
 * T\n - truncate file
 * O\n - repoen file
 * r<n>\n - set rotate count to <n>
 * b<n>\n - 1 = buffer output, 0 = don't buffer output
 */
int
main(int argc, char *argv[])
{
    int t;
    FILE *fp;
    char buf[LOGFILE_BUF_LEN];
    int rotate_count = 10;
    int do_buffer = 0;

    /* Try flushing to disk every second */
    signal(SIGALRM, signal_alarm);
    ualarm(1000000, 1000000);

    if (argc < 2) {
	printf("Error: usage: %s <logfile>\n", argv[0]);
	exit(1);
    }
    fp = fopen(argv[1], "a");
    if (fp == NULL) {
	perror("fopen");
	exit(1);
    }
    setbuf(stdout, NULL);
    close(2);
    t = open(_PATH_DEVNULL, O_RDWR);
    assert(t > -1);
    dup2(t, 2);

    while (fgets(buf, LOGFILE_BUF_LEN, stdin)) {
	/* First byte indicates what we're logging! */
	switch (buf[0]) {
	case 'L':
	    if (buf[1] != '\0') {
		fprintf(fp, "%s", buf + 1);
	    }
	    break;
	case 'R':
	    fclose(fp);
	    rotate(argv[1], rotate_count);
	    fp = fopen(argv[1], "a");
	    if (fp == NULL) {
		perror("fopen");
		exit(1);
	    }
	    break;
	case 'T':
	    break;
	case 'O':
	    break;
	case 'r':
	    //fprintf(fp, "SET ROTATE: %s\n", buf + 1);
	    rotate_count = atoi(buf + 1);
	    break;
	case 'b':
	    //fprintf(fp, "SET BUFFERED: %s\n", buf + 1);
	    do_buffer = (buf[1] == '1');
	    break;
	default:
	    /* Just in case .. */
	    fprintf(fp, "%s", buf);
	    break;
	}

	if (do_flush) {
	    do_flush = 0;
	    if (do_buffer == 0)
		fflush(fp);
	}
    }
    fclose(fp);
    fp = NULL;
    exit(0);
}

#include "include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>

#include "loghelper_commands.h"

struct {
	char *buf;
	int size;
	int used;
} inbuf;

struct {
	char *filename;
	int rotate_count;
	FILE *fp;
} logfile;

static int
inbuf_curused(void)
{
	return inbuf.used;
}

static void
inbuf_grow(int grow_by)
{
	int new_size;

	new_size = inbuf.size + grow_by;
	inbuf.buf = realloc(inbuf.buf, new_size);
	inbuf.size = new_size;
}

static int
inbuf_read(void)
{
	int ret;

	/* Grow the buffer if required */
	if (inbuf.size - inbuf.used < 1024)
		inbuf_grow(1024);

	ret = read(STDIN_FILENO, inbuf.buf + inbuf.used, inbuf.size - inbuf.used);
	if (ret <= 0) {
		fprintf(stderr, "read returned %d: %d (%s)?\n", ret, errno, strerror(errno));
		return ret;
	}
	return ret;
}

static void
inbuf_consume(int nbytes)
{
	if (nbytes > inbuf.used)
		nbytes = inbuf.used;

	if (inbuf.used < nbytes)
		memmove(inbuf.buf, inbuf.buf + nbytes, inbuf.used - nbytes);
	inbuf.used -= nbytes;
}

static void
inbuf_init(void)
{
	bzero(&inbuf, sizeof(inbuf));
}

/* Logfile stuff */


static void
logfile_init(void)
{
	bzero(&logfile, sizeof(logfile));
}

static void
logfile_set_filename(const char *fn)
{
	logfile.filename = strdup(fn);
}

static void
logfile_set_rotate_count(int rotate_count)
{
	logfile.rotate_count = rotate_count;
}

static void
logfile_open(void)
{
	if (logfile.fp != NULL)
		return;

	logfile.fp = fopen(logfile.filename, "wb+");
	/* XXX check error return? */
}

static void
logfile_close(void)
{
	if (logfile.fp == NULL)
		return;
	fclose(logfile.fp);
	logfile.fp = NULL;
}

static void
logfile_reopen(void)
{
	logfile_close();
	logfile_open();
}

/* XXX this needs to ABSOLUTELY BE REWRITTEN TO NOT BE SO HORRIBLE! */
static void
logfile_write(const char *buf, int len)
{
	int i;

	fprintf(stderr, "logfile_write: writing %d bytes\n", len);
	for (i = 0; i < len; i++)
		(void) fputc(buf[i], logfile.fp);
}

/* Command stuff */

static void
cmd_rotate(void)
{
	fprintf(stderr, "CMD_ROTATE\n");
}

static void
cmd_truncate(void)
{
	fprintf(stderr, "CMD_TRUNCATE\n");
}

static int
handle_command(u_int8_t cmd, u_int16_t len)
{
	int ret = 0;

	switch(cmd) {
		case LH_CMD_ROTATE:
			cmd_rotate();
			inbuf_consume(len);
			ret = 1;
			break;
		case LH_CMD_TRUNCATE:
			cmd_truncate();
			inbuf_consume(len);
			ret = 1;
			break;
		case LH_CMD_REOPEN:
			logfile_reopen();
			inbuf_consume(len);
			ret = 1;
			break;
		case LH_CMD_WRITE:
			/* Dirty, dirty hack */
			logfile_write(inbuf.buf + 4, len - 4);
			inbuf_consume(len);
			ret = 1;
			break;
		case LH_CMD_CLOSE:
			logfile_close();
			inbuf_consume(len);
			ret = 0;			/* EOF! */
			break;
		default:
			fprintf(stderr, "read invalid command: %d: skipping!\n", cmd);
			inbuf_consume(len);
			ret = 1;
			break;
	}
	return ret;
}

int
main(int argc, const char *argv[])
{
	u_int8_t c_cmd = 0;
	u_int16_t c_len = 0;

	int have_header = 0;

	if (argc < 2) {
		printf("error: usage: %s <logfile>\n", argv[0]);
		exit(127);
	}

	inbuf_init();
	logfile_init();
	logfile_set_filename(argv[1]);
	logfile_open();

	do {
		if (inbuf_read() <= 0)
			break;

		/* Do we have enough for the header? */
		if (have_header == 0 && inbuf_curused() < 4) {
			continue;		/* need more data */
		}

		/* If we don't have the header, snaffle it from the front */
		if (have_header == 0) {
									/* first byte is 0 for now */
			c_cmd = inbuf.buf[1];				/* command, byte */
			c_len = inbuf.buf[2] * 256 + inbuf.buf[3];	/* length of entire packet, word, network byte order */
			have_header = 1;
		}

		/* If we do have the header, call the relevant handler and see if it consumed the buffer */
		if (have_header == 1) {
			if (handle_command(c_cmd, c_len)) {
				/* The command was handled and buffer was consumed appropriately; reset for another command */
				have_header = 0;
			}
		}
	} while(1);

	logfile_close();

	exit(0);
}

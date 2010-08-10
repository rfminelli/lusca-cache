/*
 * #insert GPLv2 licence here.
 */

/*
 * This is a hackish, not-quite-finished regex external ACL helper.
 * Don't even try to use it in production yet.
 * -adrian
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <strings.h>
#include <string.h>
#include <regex.h>

#define	HELPERBUFSZ	16384
#define	MAXLINE		8192
#define	RELOAD_TIME	5

struct _regex_entry {
	int linenum;
	const char *entry;
	regex_t re;
	int re_flags;
};
typedef struct _regex_entry regex_entry_t;

struct {
	regex_entry_t *r;
	int count;
	int alloc;
} re_list = { NULL, 0, 0 };

regex_entry_t *
re_list_get(void)
{
	regex_entry_t *r;

	if (re_list.count <= re_list.alloc) {
		r = realloc(re_list.r,
		    sizeof(regex_entry_t) * (re_list.alloc + 16));
		if (r == NULL) {
			perror("re_list_get: realloc");
			return NULL;
		}
		re_list.r = r;
		re_list.alloc += 16;
	}
	
	/* Reuse r */
	r = &re_list.r[re_list.count];
	bzero(r, sizeof(regex_entry_t));
	/* The caller needs to bump re_list.count if they're using it */
	return r;
}

void
re_list_free(void)
{
	int i;
	for (i = 0; i < re_list.count; i++) {
		fprintf(stderr, "[%d]: free\n", i);
		regfree(&re_list.r[i].re);
		bzero(&re_list.r[i], sizeof(regex_entry_t));
	}
	re_list.count = 0;
}

int
regex_init(regex_entry_t *r, const char *entry, int linenum, int re_flags)
{
	int i;

	bzero(r, sizeof(*r));
	r->entry = strdup(entry);
	r->linenum = linenum;
	r->re_flags = re_flags;

	i = regcomp(&r->re, entry, re_flags);
	if (i) {	/* error condition */ 
		perror("regcomp");	/* XXX should output i instead */
		/* XXX should regfree be called here? */
		return 0;
	}

	return 1;
}

int
regex_parse_line(const char *line, int linenum)
{
	int i;
	regex_entry_t *r;

	/* Comment? skip */
	if (line[0] == '#')
		return 0;
	if (line[0] == '\r' || line[0] == '\n' || line[0] == '\0')
		return 0;

	/* Get the latest unallocated entry */
	r = re_list_get();
	if (r == NULL)
		return -1;

	/* For now, just bump the thing entirely to the line parser */
	i = regex_init(r, line, linenum, REG_EXTENDED | REG_NOSUB);
	if (i <= 0)
		return -1;

	/* success - use */
	re_list.count++;

	return 1;

}

static void
trim_trailing_crlf(char *buf)
{
	int n;

	for (n = strlen(buf) - 1;
	    n >= 0 && (buf[n] == '\r' || buf[n] == '\n');
	    n --)
		buf[n] = '\0';
}


void
load_regex_file(const char *file)
{
	FILE *fp;
	char buf[MAXLINE];
	int linenum;
	int n;

	fp = fopen(file, "r");
	if (! fp) {
		perror("fopen");
		exit(127);
	}

	linenum = 0;
	while (!feof(fp)) {
		linenum++;
		if (! fgets(buf, MAXLINE, fp))
			break;	/* XXX should check for error or EOF */

		/* Trim trailing \r\n's */
		trim_trailing_crlf(buf);
		n = regex_parse_line(buf, linenum);
		if (n > 0) {
			printf("[%d]: %s\n", linenum, buf);
		}
	}

	fclose(fp);
}

static void
check_file_update(const char *fn, struct timeval *m)
{
	/* For now, always reload */
	re_list_free();
	load_regex_file(fn);
}

static int
re_lookup(const char *url)
{
	int r, i;

	for (i = 0; i < re_list.count; i++) {
		r = regexec(&re_list.r[i].re, url, 0, NULL, 0);
		if (r == 0) {	/* Success */
			return r;
		}
	}
	return 0;
}


int
main(int argc, const char *argv[])
{
	const char *fn;
	char buf[HELPERBUFSZ];
	time_t ts;
	int r;

	if (argc < 2) {
		printf("%s: <config file>\n", argv[0]);
		exit(127);
	}
	fn = argv[1];

	/* set stdout/stderr unbuffered */
	(void) setvbuf(stdout, NULL, _IONBF, 0);
	(void) setvbuf(stderr, NULL, _IONBF, 0);

	/* initial load */
	load_regex_file(fn);
	ts = time(NULL);

	while (!feof(stdin)) {
		if (time(NULL) - ts > RELOAD_TIME) {
			ts = time(NULL);
			check_file_update(fn, NULL);
		}

		if (! fgets(buf, HELPERBUFSZ, stdin))
			break;
		trim_trailing_crlf(buf);
		fprintf(stderr, "read: %s\n", buf);
		/* XXX should break out JUST the URL here! */
		r = re_lookup(buf);
		if (r > 0) {
			fprintf(stderr, "HIT: line %d; rule %s\n",
			    re_list.r[r].linenum, re_list.r[r].entry);
			printf("YES\n");
		} else {
			printf("NO\n");
		}
	}
	re_list_free();	
	exit(0);
}

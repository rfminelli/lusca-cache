#include "config.h"

#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <dirent.h>

#include "../libcore/kb.h"
#include "../libcore/varargs.h"

#include "../libsqdebug/debug.h"

#include "../libsqtlv/tlv.h"

#include "../libsqstore/store_mgr.h"
#include "../libsqstore/store_meta.h"

#define	BUFSIZE		4096

/* normally in libiapp .. */
int shutting_down = 0;

int output = 0;

const char *
storeKeyText(const unsigned char *key)
{
	static char buf[64];
	char b2[4];

	buf[0] = '\0';

	int i;
	for (i = 0; i < 16; i++) {
		sprintf(b2, "%02X", *(key + i));
		strcat(buf, b2);
	}
	return buf;
}

void
storeMetaNew(char *buf, int len)
{
	storeMetaIndexNew *sn;

	sn = (storeMetaIndexNew *) buf;
	if (output) printf("	SWAP_META_STD_LFS: mlen %d, size %d, timestamp %ld, lastref %ld, expires %ld, lastmod %ld, file size %ld, refcount %d, flags %d\n", len, sizeof(storeMetaIndexNew), sn->timestamp, sn->lastref, sn->expires, sn->lastmod, sn->swap_file_sz, sn->refcount, sn->flags);
}

static void
parse_header(char *buf, int len)
{
	tlv *t, *tlv_list;
	int64_t *l = NULL;
	int bl = len;

	tlv_list = tlv_unpack(buf, &bl, STORE_META_END + 10);
	if (tlv_list == NULL) {
		if (output) printf("  Object: NULL\n");
		return;
	}

	/* XXX need to make sure the first entry in the list is type STORE_META_OK ? (an "int" type) */

	if (output) printf("  Object: hdr size %d\n", bl);
	for (t = tlv_list; t; t = t->next) {
	    switch (t->type) {
	    case STORE_META_URL:
		/* XXX Is this OK? Is the URL guaranteed to be \0 terminated? */
		if (output) printf("	STORE_META_URL: %s\n", (char *) t->value);
		break;
	    case STORE_META_KEY_MD5:
		if (output) printf("	STORE_META_KEY_MD5: %s\n", storeKeyText( (unsigned char *) t->value ) );
		break;
	    case STORE_META_STD_LFS:
		storeMetaNew( (char *) t->value, t->length);
		break;
	    case STORE_META_OBJSIZE:
			l = t->value;
			if (output) printf("\tSTORE_META_OBJSIZE: %" PRINTF_OFF_T " (len %d)\n", *l, t->length);
			break;
	    default:
		if (output) printf("\tType: %d; Length %d\n", t->type, (int) t->length);
	    }
	}
	if (l == NULL) {
	    //printf("  STRIPE: Completed, got an object with no size\n");
	}
	assert(tlv_list != NULL);
	tlv_free(tlv_list);
	if (output) printf("\n");
}

void
read_file(const char *path)
{
	int fd;
	char buf[BUFSIZE];
	int len;

	fd = open(path, O_RDONLY);
 	if (fd < 0) {
		perror("open");
		return;
	}
	len = read(fd, buf, BUFSIZE);
	printf("FILE: %s\n", path);
	parse_header(buf, len);
	close(fd);
}

void
read_dir(const char *dirpath)
{
	DIR *d;
	struct dirent *de;
	char path[256];

	d = opendir(dirpath);
	if (! d) {
		perror("opendir");
		return;
	}

	while ( (de = readdir(d)) != NULL) {
		if (de->d_name[0] == '.')
			continue;

		snprintf(path, sizeof(path) - 1, "%s/%s", dirpath, de->d_name);
		read_file(path);
	}
}

int
main(int argc, char *argv[])
{
    /* Setup the debugging library */
    _db_init("ALL,1");
    _db_set_stderr_debug(1);

    if (argc < 3) {
	printf("Usage: %s -f <path to swapfile>\n", argv[0]);
	printf("Usage: %s -d <directory of files to check>\n", argv[0]);
	exit(1);
    }

    if (strcmp(argv[1], "-f") == 0){
    	read_file(argv[2]);
    } else {
    	read_dir(argv[2]);
    }

    return 0;
}

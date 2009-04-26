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

#include "../include/util.h"

#include "../libcore/kb.h"
#include "../libcore/varargs.h"
#include "../libcore/mem.h"
#include "../libcore/tools.h"

#include "../libsqdebug/debug.h"

#include "../libsqtlv/tlv.h"

#define	SQUID_MD5_DIGEST_LENGTH	16

#include "../libsqstore/store_mgr.h"
#include "../libsqstore/store_meta.h"
#include "../libsqstore/store_log.h"

#define	BUFSIZE		4096

/* normally in libiapp .. */
int shutting_down = 0;

struct _rebuild_entry {
	storeMetaIndexNew mi;
	char *md5_key;
	char *url;
	char *storeurl;
	squid_file_sz file_size;
};
typedef struct _rebuild_entry rebuild_entry_t;

void
rebuild_entry_done(rebuild_entry_t *re)
{
	safe_free(re->md5_key);
	safe_free(re->url);
	safe_free(re->storeurl);
}

void
rebuild_entry_init(rebuild_entry_t *re)
{
	bzero(re, sizeof(*re));
}

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
	//if (output) printf("	SWAP_META_STD_LFS: mlen %d, size %d, timestamp %ld, lastref %ld, expires %ld, lastmod %ld, file size %ld, refcount %d, flags %d\n", len, sizeof(storeMetaIndexNew), sn->timestamp, sn->lastref, sn->expires, sn->lastmod, sn->swap_file_sz, sn->refcount, sn->flags);
}

static int
parse_header(char *buf, int len, rebuild_entry_t *re)
{
	tlv *t, *tlv_list;
	int64_t *l = NULL;
	int bl = len;
	int parsed = 0;

	tlv_list = tlv_unpack(buf, &bl, STORE_META_END + 10);
	if (tlv_list == NULL) {
		return -1;
	}

	for (t = tlv_list; t; t = t->next) {
	    switch (t->type) {
	    case STORE_META_URL:
		fprintf(stderr, "  STORE_META_URL\n");
		/* XXX Is this OK? Is the URL guaranteed to be \0 terminated? */
		re->url = xstrdup( (char *) t->value );
		parsed++;
		break;
	    case STORE_META_KEY_MD5:
		fprintf(stderr, "  STORE_META_KEY_MD5\n");
		/* XXX should double-check key length? */
		re->md5_key = xmalloc(SQUID_MD5_DIGEST_LENGTH);
		memcpy(re->md5_key, t->value, SQUID_MD5_DIGEST_LENGTH);
		parsed++;
		break;
	    case STORE_META_STD_LFS:
		fprintf(stderr, "  STORE_META_STD_LFS\n");
		/* XXX should double-check lengths match? */
		memcpy(&re->mi, t->value, sizeof(re->mi));
		parsed++;
		break;
	    case STORE_META_OBJSIZE:
		fprintf(stderr, "  STORE_META_OBJSIZE\n");
		/* XXX is this typecast'ed to the right "size" on all platforms ? */
		//re->file_size = * ((int64_t *) l);
		parsed++;
		break;
	    default:
		break;
	    }
	}
	assert(tlv_list != NULL);
	tlv_free(tlv_list);
	return (parsed > 1);
}

void
read_file(const char *path)
{
	int fd;
	char buf[BUFSIZE];
	int len;
	rebuild_entry_t re;
	storeSwapLogData sd;

	fd = open(path, O_RDONLY);
 	if (fd < 0) {
		perror("open");
		return;
	}
	len = read(fd, buf, BUFSIZE);
	fprintf(stderr, "FILE: %s\n", path);
	rebuild_entry_init(&re);
	if (parse_header(buf, len, &re)) {
		sd.op = SWAP_LOG_ADD;
		sd.swap_filen = 0x12345678;		/* XXX this should be based on the filename */
		sd.timestamp = re.mi.timestamp;
		sd.lastref = re.mi.lastref;
		sd.expires = re.mi.expires;
		sd.lastmod = re.mi.lastmod;
		sd.swap_file_sz = -1;			/* XXX for now? Need to stat the file to check */
		sd.refcount = re.mi.refcount;
		sd.flags = re.mi.flags;
		memcpy(&sd.key, re.md5_key, sizeof(sd.key));
		write(1, &sd, sizeof(sd));
	}
	rebuild_entry_done(&re);
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
    char buf[sizeof(storeSwapLogData)];
    storeSwapLogHeader *sh = (storeSwapLogHeader *) buf;

    bzero(buf, sizeof(buf));

    if (argc < 3) {
	printf("Usage: %s -f <path to swapfile>\n", argv[0]);
	printf("Usage: %s -d <directory of files to check>\n", argv[0]);
	exit(1);
    }

    /* Output swap header */
    sh->op = SWAP_LOG_VERSION;
    sh->version = 1;
    sh->record_size = sizeof(storeSwapLogData);

    write(1, sh, sizeof(storeSwapLogData));

    if (strcmp(argv[1], "-f") == 0){
    	read_file(argv[2]);
    } else {
    	read_dir(argv[2]);
    }

    return 0;
}

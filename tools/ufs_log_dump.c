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
#include "../libsqstore/store_log.h"

#define	BUFSIZE		4096

/* normally in libiapp .. */
int shutting_down = 0;

static const char *
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

static const char *
storeMetaText(storeSwapLogData *d)
{
	static char buf[1024];
	char *ks = storeKeyText(d->key);

	buf[0] = '\0';

	snprintf(buf, 1024, "op %s; fileno %X; timestamp %ld; lastref %ld; expires %ld; lastmod %ld; filesize %ld; refcount %d; flags %d; key %s",
	    swap_log_op_str[d->op],
	    (int) d->swap_filen,
	    (long int) d->timestamp,
	    (long int) d->lastref,
	    (long int) d->expires,
	    (long int) d->lastmod,
	    (long int) d->swap_file_sz,
	    (int) d->refcount,
	    (int) d->flags,
	    ks);
	return buf;
}


void
read_file(const char *path)
{
	int fd;
	char buf[BUFSIZE];
	int len;
	storeSwapLogHeader *hdr;
	storeSwapLogData *se;

	fd = open(path, O_RDONLY);
 	if (fd < 0) {
		perror("open");
		return;
	}

	/* read the header */
	len = read(fd, buf, BUFSIZE);
	if (len < 0) {
		perror("read");
		close(fd);
		return;
	}

	if (len < sizeof(*hdr)) {
		perror("too small");
		close(fd);
		return;
	}

	hdr = (storeSwapLogHeader *) buf;

	/* Which version is the swaplog? For now, we only support the -new- version */
	printf("swaplog header version: %d; record size: %d\n", hdr->version, hdr->record_size);
	printf("size of current swaplog entry: %d\n", sizeof(storeSwapLogData));

	/* The "starting" point for this record is the -record- size, not the swaplog header size. */
	if (lseek(fd, hdr->record_size, SEEK_SET) < 0) {
		perror("lseek");
		close(fd);
		return;
	}

	/* Start reading entries */
	while ((len = read(fd, buf, sizeof(storeSwapLogData))) > 0) {
		se = (storeSwapLogData *) buf;
		printf(  "Entry: %s\n", storeMetaText(se));
	}

	close(fd);
}

int
main(int argc, char *argv[])
{
    /* Setup the debugging library */
    _db_init("ALL,1");
    _db_set_stderr_debug(1);

    if (argc < 3) {
	printf("Usage: %s -f <path to swaplog>\n", argv[0]);
	exit(1);
    }

    read_file(argv[2]);

    return 0;
}

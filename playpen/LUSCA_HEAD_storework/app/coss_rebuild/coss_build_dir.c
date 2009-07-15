#include "include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <fcntl.h>

#include "include/squid_md5.h"
#include "include/util.h"
#include "libcore/tools.h"
#include "libcore/kb.h"
#include "libcore/varargs.h"
#include "libsqdebug/debug.h"
#include "libsqstore/store_mgr.h"
#include "libsqstore/store_log.h"
#include "libsqstore/store_meta.h"
#include "libsqtlv/tlv.h"

/*
 * Rebuilding from the COSS filesystem itself is currently very, very
 * resource intensive. Since there is no on-disk directory, the
 * whole store must first be read from start to finish and objects
 * must be pulled out of the stripes.
 *
 * There's also no easy logic to determine where the -head- pointer of
 * the filesystem was.
 *
 * All in all this is quite a horrible method for rebuilding..
 */

#define	DEFAULT_BLOCKSIZE	1024
#define	DEFAULT_BLOCKBITS	10
#define	DEFAULT_STRIPESIZE	1048576

size_t	stripe_size = 0;	/* XXX needs to be changed, obviously */
int	stripe_blksize = 1024;


static void
parse_stripe(int stripeid, char *buf, int len, int blocksize, size_t stripesize)
{   
	int j = 0;
	int bl = 0;
	tlv *t, *tlv_list;
	int64_t *l;
	int tmp;

	while (j < len) {
		l = NULL;
		bl = 0;
		tlv_list = tlv_unpack(&buf[j], &bl, STORE_META_END + 10);
		if (tlv_list == NULL) {
			printf("  Object: NULL\n");
			return;
		}
		printf("  Object: (filen %d) hdr size %d\n", j / blocksize + (stripeid * stripesize / blocksize), bl);
		for (t = tlv_list; t; t = t->next) {
			switch (t->type) {
			case STORE_META_URL:
				/* XXX Is this OK? Is the URL guaranteed to be \0 terminated? */
				printf("	URL: %s\n", (char *) t->value);
				break;
			case STORE_META_OBJSIZE:
				l = t->value;
				printf("Size: %" PRINTF_OFF_T " (len %d)\n", *l, t->length);
				break;
			}
		}
		if (l == NULL) {
			printf("  STRIPE: Completed, got an object with no size\n");
			return;
		}
		j = j + *l + bl;
		/* And now, the blocksize! */
		tmp = j / blocksize;
		tmp = (tmp + 1) * blocksize;
		j = tmp;

		tlv_free(tlv_list);
	}
}

int
coss_rebuild_dir(const char *file, size_t stripesize, int blocksize, int numstripes)
{
	int fd;
	char *buf;
	int i = 0, len;
	int blksize_bits;

	buf = malloc(stripesize);
	if (! buf) {
		debug(1, 1) ("%s: couldn't allocated %d bytes for rebuild buffer: (%d) %s\n", file, stripesize, errno, xstrerror());
		return 0;
	}

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 0;
	}

	for(blksize_bits = 0;((blocksize >> blksize_bits) > 0);blksize_bits++) {
		if( ((blocksize >> blksize_bits) > 0) &&
		  (((blocksize >> blksize_bits) << blksize_bits) != blocksize)) {
			debug(1, 1) ("%s: Blocksize bits (%d) must be a power of 2\n", file, blksize_bits);
			safe_free(buf);
			return(0);
		}
	}

	while ((len = read(fd, buf, stripesize)) > 0) {
		printf("STRIPE: %d (len %d)\n", i, len);
		parse_stripe(i, buf, len, blocksize, stripesize);
		i++;
		if((numstripes > 0) && (i >= numstripes))
			break;
	}
	close(fd);

	safe_free(buf);
	return 1;
}

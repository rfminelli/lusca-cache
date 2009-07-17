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
#include "libsqtlv/tlv.h"
#include "libsqstore/store_mgr.h"
#include "libsqstore/store_log.h"
#include "libsqstore/store_meta.h"
#include "libsqstore/rebuild_entry.h"


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

static void
parse_stripe(int stripeid, char *buf, int len, int blocksize, size_t stripesize)
{   
	int j = 0;
	int tmp;
	rebuild_entry_t re;

	while (j < len) {
		rebuild_entry_init(&re);
		if (! parse_header(&buf[j], len - j, &re)) {
			rebuild_entry_done(&re);
			debug(85, 5) ("parse_stripe: id %d: no more data or invalid header\n", stripeid);
			return;
		}

		debug(85, 5) ("  Object: (filen %d)\n", j / blocksize + (stripeid * stripesize / blocksize));
		debug(85, 5) ("  URL: %s\n", re.url);
		debug(85, 5) ("  hdr_size: %d\n", (int) re.hdr_size);
		debug(85, 5) ("  file_size: %d\n", (int) re.file_size);

		/*
		 * We require at least the size to continue. If we don't get a valid size to read the next
		 * object for, we can't generate a swaplog entry. Leave checking consistency up to the
		 * caller.
		 */
		if (re.hdr_size == -1 || re.file_size == -1) {
			rebuild_entry_done(&re);
			debug(85, 5) ("parse_stripe: id %d: not enough information in this object; end of stripe?\n", stripeid);
			return;
		}

		j = j + re.file_size + re.hdr_size;
		/* And now, the blocksize! */
		tmp = j / blocksize;
		tmp = (tmp + 1) * blocksize;
		j = tmp;
		rebuild_entry_done(&re);
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
		debug(85, 1) ("%s: couldn't allocated %d bytes for rebuild buffer: (%d) %s\n", file, stripesize, errno, xstrerror());
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
			debug(85, 1) ("%s: Blocksize bits (%d) must be a power of 2\n", file, blksize_bits);
			safe_free(buf);
			return(0);
		}
	}

	while ((len = read(fd, buf, stripesize)) > 0) {
		debug(85, 5) ("STRIPE: %d (len %d)\n", i, len);
		parse_stripe(i, buf, len, blocksize, stripesize);
		i++;
		if((numstripes > 0) && (i >= numstripes))
			break;
	}
	close(fd);

	safe_free(buf);
	return 1;
}

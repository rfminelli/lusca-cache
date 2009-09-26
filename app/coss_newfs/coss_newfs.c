#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "include/config.h"
#include "include/util.h"

#include "libcore/varargs.h"
#include "libcore/tools.h"
#include "libsqdebug/debug.h"

int shutting_down = 0;	/* needed for debug routines for now */

/*
 * Args: /path/to/cossdir <number of stripes> <stripesize>
 */
int
main(int argc, const char *argv[])
{
	size_t sz, stripe_sz;
	const char *path;
	size_t i;
	int fd;
	char buf[256];
	off_t r;

        /* Setup the debugging library */
        _db_init("ALL,1");
        _db_set_stderr_debug(1);

	if (argc < 3) {
		printf("Usage: %s <path> <stripe count> <stripe size>\n", argv[0]);
		exit(1);
	}

	path = argv[1];
	sz = atoi(argv[2]);
	stripe_sz = atoi(argv[3]);

	/*
	 * For now, just write 256 bytes of NUL's into the beginning of
	 * each stripe. COSS doesn't really have an on-disk format
	 * that leads itself to anything newfs-y quite yet. The NULs
	 * -should- be enough to trick the rebuild process into treating
	 * the rest of that stripe as empty.
	 */
	fd = open(path, O_WRONLY | O_CREAT, 0644);
	if (fd < 0) {
		perror("open");
		exit(127);
	}
	bzero(buf, sizeof(buf));

	for (i = 0; i < sz; i += 1) {
		getCurrentTime();
		debug(85, 5) ("seeking to stripe %d\n", i);
		r = lseek(fd, (off_t) i * (off_t) stripe_sz, SEEK_SET);
		if (r < 0) {
			perror("lseek");
			exit(127);
		}
		r = write(fd, buf, sizeof(buf));
		if (r < 0) {
			perror("write");
			exit(127);
		}
	}

	close(fd);

	exit(0);
}

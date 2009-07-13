#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "include/config.h"
#include "include/util.h"

#include "libcore/tools.h"
#include "libcore/varargs.h"
#include "libsqdebug/debug.h"

int shutting_down = 0;	/* needed for debug routines for now */

/*
 * Args: /path/to/cossdir <size> <stripesize>
 *
 * All arguments are in megabytes for now.
 */
int
main(int argc, const char *argv[])
{
	size_t sz, stripe_sz;
	const char *path;
	size_t i;
	int fd;
	char buf[256];
	int r;

	path = argv[1];

	sz = atoi(argv[2]) * 1024;
	stripe_sz = atoi(argv[3]) * 1024;

	/*
	 * For now, just write 256 bytes of NUL's into the beginning of
	 * each stripe. COSS doesn't really have an on-disk format
	 * that leads itself to anything newfs-y quite yet. The NULs
	 * -should- be enough to trick the rebuild process into treating
	 * the rest of that stripe as empty.
	 */
	fd = open(path, O_WRONLY | O_CREAT);
	if (fd < 0) {
		perror("open");
		exit(127);
	}
	bzero(buf, sizeof(buf));

	for (i = 0; i < sz; i += stripe_sz) {
		r = lseek(fd, i, SEEK_SET);
		if (r < 0) {
			perror("lseek");
			exit(127);
		}
		r = write(fd, buf, sizeof(buf));
		if (r < 0) {
			perror("lseek");
			exit(127);
		}
	}

	close(fd);

	exit(0);
}

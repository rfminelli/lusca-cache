#include "include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libcore/tools.h"
#include "libsqdebug/debug.h"

#include "coss_build_dir.h"

int shutting_down = 0;	/* needed for libiapp */

int
main(int argc, const char *argv[])
{
	const char *cmd;
	const char *path;
	int block_size;
	size_t stripe_size;
	int num_stripes;

	/* Setup the debugging library */
	_db_init("ALL,1");
	_db_set_stderr_debug(1);

	cmd = argv[1];
	path = argv[2];
	block_size = atoi(argv[3]);
	stripe_size = atoi(argv[4]);
	num_stripes = atoi(argv[5]);

	(void) coss_rebuild_dir(path, stripe_size, block_size, num_stripes);

	exit(0);
}

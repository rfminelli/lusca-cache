#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libcore/varargs.h"
#include "libsqdebug/debug.h"
#include "libsqstore/store_file_ufs.h"

int shutting_down = 0;

int
main(int argc, char *argv[])
{
	/* Setup the debugging library */
	_db_init("ALL,1");
	_db_set_stderr_debug(1);
	store_ufs_dir_t store_ufs_info;

	if (argc < 5) {
		printf("Usage: %s <store path> <l1> <l2> <path to swapfile>\n", argv[0]);
		exit(1);
	}

	store_ufs_init(&store_ufs_info, argv[1], atoi(argv[2]), atoi(argv[3]), argv[4]);

	/* Output swap header to stdout */
	(void) storeSwapLogPrintHeader(1);

	read_dir(&store_ufs_info);
	store_ufs_done(&store_ufs_info);

	return 0;
}

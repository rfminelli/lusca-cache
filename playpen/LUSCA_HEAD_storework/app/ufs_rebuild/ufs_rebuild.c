#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "include/config.h"
#include "include/squid_md5.h"

#include "libcore/varargs.h"
#include "libcore/kb.h"
#include "libsqdebug/debug.h"
#include "libsqstore/store_mgr.h"
#include "libsqstore/store_log.h"
#include "libsqstore/store_file_ufs.h"

#include "ufs_build_dir.h"
#include "ufs_build_log.h"

int shutting_down = 0;

typedef enum {
	REBUILD_NONE,
	REBUILD_DISK,
	REBUILD_LOG
} rebuild_type_t;

int
main(int argc, char *argv[])
{
	/* Setup the debugging library */
	_db_init("ALL,1");
	_db_set_stderr_debug(1);
	store_ufs_dir_t store_ufs_info;
	rebuild_type_t rebuild_type;

	if (argc < 5) {
		printf("Usage: %s <store path> <l1> <l2> <path to swapfile>\n", argv[0]);
		exit(1);
	}

	store_ufs_init(&store_ufs_info, argv[1], atoi(argv[2]), atoi(argv[3]), argv[4]);

	rebuild_type = REBUILD_DISK;

	/* Output swap header to stdout */
	(void) storeSwapLogPrintHeader(1);

	if (rebuild_type == REBUILD_DISK)
		rebuild_from_dir(&store_ufs_info);
	else
		rebuild_from_log(&store_ufs_info);

	store_ufs_done(&store_ufs_info);

	return 0;
}

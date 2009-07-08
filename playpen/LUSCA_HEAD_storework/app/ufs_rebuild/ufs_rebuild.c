#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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

static rebuild_type_t
probe_dir(store_ufs_dir_t *u)
{
	if (store_ufs_has_valid_rebuild_log(u))
		return REBUILD_LOG;
	return REBUILD_DISK;
}

static void
usage(const char *cmdname)
{
	printf("Usage: %s <command> <store path> <l1> <l2> <path to swapfile>\n", cmdname);
	printf("  where <command> is one of rebuild-dir, rebuild-log or rebuild.\n");
}


int
main(int argc, char *argv[])
{
	const char *cmd;
	store_ufs_dir_t store_ufs_info;
	rebuild_type_t rebuild_type;

	/* Setup the debugging library */
	_db_init("ALL,1");
	_db_set_stderr_debug(1);

	if (argc < 5) {
		usage(argv[0]);
		exit(1);
	}
	cmd = argv[1];

	store_ufs_init(&store_ufs_info, argv[2], atoi(argv[3]), atoi(argv[4]), argv[5]);

	if (strcmp(cmd, "rebuild-dir") == 0) {
		rebuild_type = REBUILD_DISK;
	} else if (strcmp(cmd, "rebuild-log") == 0) {
		rebuild_type = REBUILD_LOG;
	} else if (strcmp(cmd, "rebuild") == 0) {
		rebuild_type = probe_dir(&store_ufs_info);
	} else {
		usage(argv[0]);
		exit(1);
	}

	/* Output swap header to stdout */
	(void) storeSwapLogPrintHeader(1);

	if (rebuild_type == REBUILD_DISK)
		rebuild_from_dir(&store_ufs_info);
	else
		rebuild_from_log(&store_ufs_info);

	store_ufs_done(&store_ufs_info);
	(void) storeSwapLogPrintCompleted(1);

	return 0;
}

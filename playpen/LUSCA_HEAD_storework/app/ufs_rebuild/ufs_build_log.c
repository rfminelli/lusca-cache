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

#include <errno.h>
#include <sys/stat.h>

#include "include/util.h"
#include "include/squid_md5.h"

#include "libcore/kb.h"
#include "libcore/varargs.h"
#include "libcore/mem.h"
#include "libcore/tools.h"

#include "libsqdebug/debug.h"

#include "libsqtlv/tlv.h"

#include "libsqstore/store_mgr.h"
#include "libsqstore/store_log.h"
#include "libsqstore/store_file_ufs.h"

int num_objects = 0;
int num_valid_objects = 0;
int num_invalid_objects = 0;

static int
rebuild_log_progress(FILE *fp, size_t s, int num_objects)
{
	struct stat sb;
	if (0 == fstat(fileno(fp), &sb)) {
		if (! storeSwapLogPrintProgress(1, num_objects, (int) sb.st_size / s))
			return 0;
	}
	return 1;
}

static size_t
rebuild_entry_size(int version)
{
	if (version == 1)
		return sizeof(storeSwapLogData);
	return sizeof(storeSwapLogDataOld);
}

int
read_entry(FILE *fp, int version)
{
	int r;
	char buf[128];
	storeSwapLogData sd;

	r = fread(buf, rebuild_entry_size(version), 1, fp);
	if (r != 1) {
		debug(1, 2) ("fread: returned %d (ferror %d)\n", r, ferror(fp));
		return 0;
	}
	num_objects++;

	if ((num_objects & 0xffff) == 0) {
		if (! rebuild_log_progress(fp, rebuild_entry_size(version), num_objects))
			return 0;
	}

	/* Decode the entry */
	if (version == 1) {
		memcpy(&sd, buf, sizeof(sd));
	} else {
		(void) storeSwapLogUpgradeEntry(&sd, (storeSwapLogDataOld *) buf);
	}

	/* is it an ADD/DEL? Good. If not - count error and continue */
	if (sd.op == SWAP_LOG_ADD || sd.op == SWAP_LOG_DEL) {
		num_valid_objects++;
		if (write(1, &sd, sizeof(sd)) <= 0) {
			debug(47, 1) ("write failed: (%d) %s\n", errno, xstrerror());
			return 0;
		}
	} else {
		debug(1, 5) ("error! Got swaplog entry op %d?!\n", sd.op);
		num_invalid_objects++;
	}

	return 1;
}

void
rebuild_from_log(store_ufs_dir_t *ufs)
{
	FILE *fp;
	storeSwapLogHeader hdr;
	int r;
	int version = -1;		/* -1 = not set, 0 = old, 1 = new */

	fp = fopen(ufs->swaplog_path, "r");
	if (! fp) {
		perror("fopen");
		return;
	}

	/* Read an entry - see if its a swap header! */
	r = fread(&hdr, sizeof(hdr), 1, fp);
	if (r != 1) {
		perror("fread");
		fclose(fp);
		return;
	}

	/*
	 * If hdr is a swap log version then we can determine whether
	 * if its old or new. If it isn't then we can't determine what
	 * type it is, so assume its the "default" size.
	 */
	if (hdr.op == SWAP_LOG_VERSION) {
		if (fseek(fp, hdr.record_size, SEEK_SET) != 0) {
			perror("fseek");
			fclose(fp);
			return;
		}
		if (hdr.version == 1 && hdr.record_size == sizeof(storeSwapLogData)) {
			version = 1;
		} else if (hdr.version == 1 && hdr.record_size == sizeof(storeSwapLogDataOld)) {
			version = 0;
		} else {
			debug(1, 1) ("Unsupported swap.state version %d size %d\n", hdr.version, hdr.record_size);
			fclose(fp);
			return;
		}
	} else {
		/* Otherwise, default based on the compile time size comparison */
		rewind(fp);
#if SIZEOF_SQUID_FILE_SZ == SIZEOF_SIZE_T
		version = 1;
#else
		version = 0;
#endif
	}

	/* begin echo'ing the log info */
	storeSwapLogPrintHeader(1);	/* to stdout */

	/* Now - loop over until eof or error */
	while (! feof(fp)) {
		if (! read_entry(fp, version)) {
			break;
		}
	}

	/* Queue a final progress update */
	if (! rebuild_log_progress(fp, rebuild_entry_size(version), num_objects))
		return;

	fclose(fp);
}

#if 0
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

    read_log_file(argv[2]);

    debug(1, 1) ("%s: Read %d objects\n", argv[2], num_objects);

    return 0;
}
#endif

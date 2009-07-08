
/*
 * This program provides the "rebuild" logic for a UFS spool.
 *
 * It will scan a UFS style directory for valid looking swap files
 * and spit out a new style swap log to STDOUT.
 *
 * Adrian Chadd <adrian@creative.net.au>
 */

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

#include <sys/stat.h>
#include <dirent.h>

#include "include/util.h"

#include "libcore/kb.h"
#include "libcore/varargs.h"
#include "libcore/mem.h"
#include "libcore/tools.h"

#include "libsqdebug/debug.h"

#define	SQUID_MD5_DIGEST_LENGTH	16

#include "libsqstore/store_mgr.h"
#include "libsqstore/store_meta.h"
#include "libsqstore/store_log.h"
#include "libsqstore/store_file_ufs.h"

#include "rebuild_entry.h"

#define	BUFSIZE		1024

int
read_file(const char *path, rebuild_entry_t *re)
{
	int fd;
	char buf[BUFSIZE];
	int len;
	struct stat sb;

	debug(47, 3) ("read_file: %s\n", path);
	fd = open(path, O_RDONLY);
 	if (fd < 0) {
		perror("open");
		return -1;
	}

	/* We need the entire file size */
	if (fstat(fd, &sb) < 0) {
		perror("fstat");
		return -1;
	}

	len = read(fd, buf, BUFSIZE);
	debug(47, 3) ("read_file: FILE: %s\n", path);

	if (! parse_header(buf, len, re)) {
		close(fd);
		return -1;
	}
	re->file_size = sb.st_size;
	close(fd);
	return 1;
}

int
write_swaplog_entry(rebuild_entry_t *re)
{
	storeSwapLogData sd;

	sd.op = SWAP_LOG_ADD;
	sd.swap_filen = re->swap_filen;
	sd.timestamp = re->mi.timestamp;
	sd.lastref = re->mi.lastref;
	sd.expires = re->mi.expires;
	sd.lastmod = re->mi.lastmod;
	sd.swap_file_sz = re->file_size;
	sd.refcount = re->mi.refcount;
	sd.flags = re->mi.flags;

	memcpy(&sd.key, re->md5_key, sizeof(sd.key));
	if (! write(1, &sd, sizeof(sd)))
		return -1;

	return 1;
}

void
read_dir(store_ufs_dir_t *sd)
{
	DIR *d;
	struct dirent *de;
	char path[SQUID_MAXPATHLEN];
	char dir[SQUID_MAXPATHLEN];
	rebuild_entry_t re;
	int fn;
	int i, j;

	getCurrentTime();
	for (i = 0; i < store_ufs_l1(sd); i++) {
		for (j = 0; j < store_ufs_l2(sd); j++) {
			(void) store_ufs_createDir(sd, i, j, dir);
			getCurrentTime();
			debug(47, 1) ("read_dir: opening dir %s\n", dir);
			d = opendir(dir);
			if (! d) {
				perror("opendir");
				continue;
			}

			while ( (de = readdir(d)) != NULL) {
				if (de->d_name[0] == '.')
					continue;
				getCurrentTime();

				/* Verify that the given filename belongs in the given directory */
				if (sscanf(de->d_name, "%x", &fn) != 1) {
					debug(47, 1) ("read_dir: invalid %s\n", de->d_name);
						continue;
				}
				if (! store_ufs_filenum_correct_dir(sd, fn, i, j)) {
					debug(47, 1) ("read_dir: %s does not belong in %d/%d\n", de->d_name, i, j);
						continue;
				}

				snprintf(path, sizeof(path) - 1, "%s/%s", dir, de->d_name);
				debug(47, 3) ("read_dir: opening %s\n", path);

				rebuild_entry_init(&re);
				(void) read_file(path, &re);
				re.swap_filen = fn;
				(void) write_swaplog_entry(&re);
				rebuild_entry_done(&re);

			}
			closedir(d);
		}
	}
}

#if 0
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
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "../include/config.h"
#include "../include/squid_md5.h"
#include "../include/util.h"

#include "../libcore/varargs.h"
#include "../libcore/kb.h"
#include "../libcore/tools.h"	/* for SQUID_MAXPATHLEN */

#include "store_mgr.h"
#include "store_log.h"

#include "store_file_ufs.h"

void
store_ufs_init(store_ufs_dir_t *sd, const char *path, int l1, int l2)
{
	sd->path = xstrdup(path);
	sd->l1 = l1;
	sd->l2 = l2;
}

void
store_ufs_done(store_ufs_dir_t *sd)
{
	safe_free(sd->path);
}

/*
 * Create a UFS path given the component bits.
 *
 * "buf" must be SQUID_MAXPATHLEN.
 */
int
store_ufs_createPath(store_ufs_dir_t *sd, int filn, char *buf)
{   
    int L1 = store_ufs_l1(sd);
    int L2 = store_ufs_l2(sd);
    buf[0] = '\0';
    snprintf(buf, SQUID_MAXPATHLEN, "%s/%02X/%02X/%08X",
        store_ufs_path(sd),
        ((filn / L2) / L2) % L1,
        (filn / L2) % L2,
        filn);
    return 1;
}   

/*
 * Create a UFS directory path given the component bits.
 */
int
store_ufs_createDir(store_ufs_dir_t *sd, int i, int j, char *buf)
{
    buf[0] = '\0';
    snprintf(buf, SQUID_MAXPATHLEN, "%s/%02X/%02X", store_ufs_path(sd), i, j);
    return 1;
}

/*
 * F1/F2 - current directory numbers which they're in
 * L1/L2 - configured storedir L1/L2
 * fn - file number
 *
 * returns whether "fn" belongs in the directory F1/F2 given the configured L1/L2
 */
int
store_ufs_filenum_correct_dir(store_ufs_dir_t *sd, int fn, int F1, int F2)
{
    int L1 = store_ufs_l1(sd), L2 = store_ufs_l2(sd);
    int D1, D2;
    int filn = fn;

    D1 = ((filn / L2) / L2) % L1;
    if (F1 != D1)
        return 0;
    D2 = (filn / L2) % L2;
    if (F2 != D2)
        return 0;
    return 1;
}

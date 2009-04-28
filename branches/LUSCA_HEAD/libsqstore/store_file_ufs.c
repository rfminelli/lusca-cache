#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "../include/config.h"
#include "../include/squid_md5.h"

#include "../libcore/varargs.h"
#include "../libcore/kb.h"
#include "../libcore/tools.h"	/* for SQUID_MAXPATHLEN */

#include "store_mgr.h"
#include "store_log.h"

#include "store_file_ufs.h"

/*
 * Create a UFS path given the component bits.
 *
 * "buf" must be SQUID_MAXPATHLEN.
 */
int
store_ufs_createPath(const char *prefix, int filn, int L1, int L2, char *buf)
{   
    buf[0] = '\0';
    snprintf(buf, SQUID_MAXPATHLEN, "%s/%02X/%02X/%08X",
        prefix,
        ((filn / L2) / L2) % L1,
        (filn / L2) % L2,
        filn);
    return 1;
}   

/*
 * Create a UFS directory path given the component bits.
 */
int
store_ufs_createDir(const char *prefix, int L1, int L2, char *buf)
{
    buf[0] = '\0';
    snprintf(buf, SQUID_MAXPATHLEN, "%s/%02X/%02X", prefix, L1, L2);
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
store_ufs_filenum_correct_dir(int fn, int F1, int F2, int L1, int L2)
{
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


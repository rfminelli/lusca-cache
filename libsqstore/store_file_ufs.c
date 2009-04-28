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

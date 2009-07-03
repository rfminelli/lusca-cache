#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libcore/tools.h"
#include "libcore/varargs.h"
#include "libsqdebug/debug.h"
#include "libsqstore/store_file_ufs.h"

int shutting_down = 0;	/* needed for debug routines for now */

int  
storeAufsDirCreateDirectory(const char *path, int should_exist)
{  
    int created = 0;
    struct stat st;
    getCurrentTime();
    if (0 == stat(path, &st)) {
        if (S_ISDIR(st.st_mode)) {
            debug(47, should_exist ? 3 : 1) ("%s exists\n", path);
        } else {
            libcore_fatalf("Swap directory %s is not a directory.\n", path);
        }
#ifdef _SQUID_MSWIN_
    } else if (0 == mkdir(path)) {
#else
    } else if (0 == mkdir(path, 0755)) {
#endif
        debug(47, should_exist ? 1 : 3) ("%s created\n", path);
        created = 1;
    } else {
        libcore_fatalf("Failed to make swap directory %s: %s\n", path, xstrerror());
    }
    return created;
}

void
storeAufsDirCreateSwapSubDirs(store_ufs_dir_t *sd)
{   
    int i, k;
    int should_exist;
    char name[MAXPATHLEN];

    for (i = 0; i < sd->l1; i++) {
        snprintf(name, MAXPATHLEN, "%s/%02X", sd->path, i);
        if (storeAufsDirCreateDirectory(name, 0))
            should_exist = 0;
        else
            should_exist = 1;
        debug(47, 1) ("Making directories in %s\n", name);
        for (k = 0; k < sd->l2; k++) {
            snprintf(name, MAXPATHLEN, "%s/%02X/%02X", sd->path, i, k);
            storeAufsDirCreateDirectory(name, should_exist);
        }
    }
}

int
main(int argc, const char *argv[])
{
	store_ufs_dir_t sd;

	store_ufs_init(&sd, argv[1], atoi(argv[2]), atoi(argv[3]), "/tmp/f");
	storeAufsDirCreateSwapSubDirs(&sd);
	store_ufs_done(&sd);

	exit(0);
}

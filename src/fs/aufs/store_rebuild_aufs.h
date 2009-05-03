#ifndef	__STORE_REBUILD_AUFS_H__
#define	__STORE_REBUILD_AUFS_H__

typedef struct _RebuildState RebuildState;
struct _RebuildState {
    SwapDir *sd;
    int n_read;
    int log_fd;
    int speed;
    int curlvl1;
    int curlvl2;
    struct {
        unsigned int clean:1;
        unsigned int init:1;
        unsigned int old_swaplog_entry_size:1;
    } flags;
    int done;
    int in_dir;
    int fn;
    struct dirent *entry;
    DIR *td;
    char fullpath[SQUID_MAXPATHLEN];
    char fullfilename[SQUID_MAXPATHLEN];
    struct _store_rebuild_data counts;
};



#endif

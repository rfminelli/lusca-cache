#ifndef	__STORE_REBUILD_COSS_H__
#define	__STORE_REBUILD_COSS_H__

typedef struct _RebuildState RebuildState;
struct _RebuildState {
    SwapDir *sd;
    int n_read;
    FILE *log;
    int report_interval;
    int report_current;
    struct {
        unsigned int clean:1;
    } flags;
    struct _store_rebuild_data counts;
    struct {
        int new;
        int reloc;
        int fresher;
        int unknown;
    } cosscounts;
};

extern void storeCossDirRebuild(SwapDir * sd);

#endif

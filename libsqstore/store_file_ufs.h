#ifndef	__LIBSQSTORE_STORE_FILE_UFS_H__
#define	__LIBSQSTORE_STORE_FILE_UFS_H__

struct _store_ufs_dir {
	char *path;
	int l1;
	int l2;
};
typedef struct _store_ufs_dir store_ufs_dir_t;

extern void store_ufs_init(store_ufs_dir_t *sd, const char *path, int l1, int l2);
extern void store_ufs_done(store_ufs_dir_t *sd);

extern int store_ufs_createPath(const char *prefix, int swap_filen, int L1, int L2, char *buf);
extern int store_ufs_createDir(const char *prefix, int L1, int L2, char *buf);
extern int store_ufs_filenum_correct_dir(int fn, int F1, int F2, int L1, int L2);


#endif

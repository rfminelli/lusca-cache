#ifndef	__LIBSQSTORE_STORE_FILE_UFS_H__
#define	__LIBSQSTORE_STORE_FILE_UFS_H__

extern int store_ufs_createPath(const char *prefix, int swap_filen, int L1, int L2, char *buf);
extern int store_ufs_createDir(const char *prefix, int L1, int L2, char *buf);
extern int store_ufs_filenum_correct_dir(int fn, int F1, int F2, int L1, int L2);


#endif

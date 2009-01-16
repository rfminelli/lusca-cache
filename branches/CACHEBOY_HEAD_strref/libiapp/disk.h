#ifndef	__LIBIAPP_DISK_H__
#define	__LIBIAPP_DISK_H__

extern int file_open(const char *path, int mode);
extern void file_close(int fd);
extern void file_write(int, off_t, void *, size_t len, DWCB *, void *, FREE *);
extern void file_write_mbuf(int fd, off_t, MemBuf mb, DWCB * handler, void *handler_data);
extern void file_read(int, char *, size_t, off_t, DRCB *, void *);
extern void disk_init(void);
extern void disk_init_mem(void); 

#endif

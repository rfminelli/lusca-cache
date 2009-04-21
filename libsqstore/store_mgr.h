#ifndef	__LIBSQSTORE_STORE_MGR_H__
#define	__LIBSQSTORE_STORE_MGR_H__


typedef unsigned int store_status_t;
typedef unsigned int mem_status_t;
typedef unsigned int ping_status_t;
typedef unsigned int swap_status_t;
typedef signed int sfileno;
typedef signed int sdirno;

#if LARGE_CACHE_FILES
typedef squid_off_t squid_file_sz;
#define SIZEOF_SQUID_FILE_SZ SIZEOF_SQUID_OFF_T
#else
typedef size_t squid_file_sz;
#define SIZEOF_SQUID_FILE_SZ SIZEOF_SIZE_T
#endif

#endif

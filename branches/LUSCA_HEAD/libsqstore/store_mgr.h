#ifndef	__LIBSQSTORE_STORE_MGR_H__
#define	__LIBSQSTORE_STORE_MGR_H__

#define STORE_META_OK     0x03
#define STORE_META_DIRTY  0x04
#define STORE_META_BAD    0x05

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

/*
 * NOTE!  We must preserve the order of this list!
 */
typedef enum {
    STORE_META_VOID,            /* should not come up */
    STORE_META_KEY_URL,         /* key w/ keytype */
    STORE_META_KEY_SHA,
    STORE_META_KEY_MD5,
    STORE_META_URL,             /* the url , if not in the header */
    STORE_META_STD,             /* standard metadata */
    STORE_META_HITMETERING,     /* reserved for hit metering */
    STORE_META_VALID,
    STORE_META_VARY_HEADERS,    /* Stores Vary request headers */
    STORE_META_STD_LFS,         /* standard metadata in lfs format */
    STORE_META_OBJSIZE,         /* object size, if its known */
    STORE_META_STOREURL,        /* the store url, if different to the normal URL */
    STORE_META_END
} store_meta_types;

#endif

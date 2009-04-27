#ifndef	__LIBSQSTORE_STORE_LOG_H__
#define	__LIBSQSTORE_STORE_LOG_H__

typedef enum {
    SWAP_LOG_NOP,
    SWAP_LOG_ADD,
    SWAP_LOG_DEL,
    SWAP_LOG_VERSION,
    SWAP_LOG_MAX
} swap_log_op;

/*
 * Do we need to have the dirn in here? I don't think so, since we already
 * know the dirn ..
 */
struct _storeSwapLogData {
    char op;
    sfileno swap_filen;
    time_t timestamp;
    time_t lastref;
    time_t expires;
    time_t lastmod;
    squid_file_sz swap_file_sz;
    u_short refcount;
    u_short flags;
    unsigned char key[SQUID_MD5_DIGEST_LENGTH];
};
typedef struct _storeSwapLogData storeSwapLogData;

struct _storeSwapLogHeader {
    char op;
    int version;
    int record_size;
};
typedef struct _storeSwapLogHeader storeSwapLogHeader;

#if SIZEOF_SQUID_FILE_SZ != SIZEOF_SIZE_T
struct _storeSwapLogDataOld {
    char op;
    sfileno swap_filen;
    time_t timestamp;
    time_t lastref;
    time_t expires;
    time_t lastmod;
    size_t swap_file_sz;
    u_short refcount;
    u_short flags;
    unsigned char key[SQUID_MD5_DIGEST_LENGTH];
};
typedef struct _storeSwapLogDataOld storeSwapLogDataOld;
#endif

extern const char * swap_log_op_str[];

extern int storeSwapLogUpgradeEntry(storeSwapLogData *dst, storeSwapLogDataOld *src);

#endif

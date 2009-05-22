#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

#include "../include/config.h"
#include "../include/squid_md5.h"

#include "../libcore/varargs.h"
#include "../libcore/kb.h"

#include "store_mgr.h"
#include "store_log.h"

const char * swap_log_op_str[] = {
    "SWAP_LOG_NOP",
    "SWAP_LOG_ADD",
    "SWAP_LOG_DEL",
    "SWAP_LOG_VERSION",
    "SWAP_LOG_MAX"
};

int
storeSwapLogUpgradeEntry(storeSwapLogData *dst, storeSwapLogDataOld *src)
{
    dst->op = src->op;
    dst->swap_filen = src->swap_filen;
    dst->timestamp = src->timestamp;
    dst->lastref = src->lastref;
    dst->expires = src->expires;
    dst->lastmod = src->lastmod;
    dst->swap_file_sz = src->swap_file_sz;			/* This is the entry whose size has changed */
    dst->refcount = src->refcount;
    dst->flags = src->flags;
    memcpy(dst->key, src->key, SQUID_MD5_DIGEST_LENGTH);

    return 1;
}

int
storeSwapLogPrintHeader(int fd)
{
    char buf[sizeof(storeSwapLogData)];
    storeSwapLogHeader *sh = (storeSwapLogHeader *) buf;

    bzero(buf, sizeof(buf));
    sh->op = SWAP_LOG_VERSION;
    sh->version = 1;
    sh->record_size = sizeof(storeSwapLogData);
    return write(1, sh, sizeof(storeSwapLogData));
}


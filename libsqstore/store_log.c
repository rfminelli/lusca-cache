#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "../include/config.h"
#include "../include/squid_md5.h"

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



#ifndef _PROFILING_H_
#define _PROFILING_H_

#include "config.h"

#ifdef USE_XPROF_STATS

#if !defined(_SQUID_SOLARIS_)
typedef long long hrtime_t;
#else
#include <sys/time.h>
#endif

#if defined(__i386)
static inline hrtime_t
get_tick(void)
{
    hrtime_t regs;
    asm volatile ("rdtsc":"=A" (regs));
    return regs;
    /* We need return value, we rely on CC to optimise out needless subf calls */
    /* Note that "rdtsc" is relatively slow OP and stalls the CPU pipes, so use it wisely */
}

#elif defined(__alpha)
static inline hrtime_t
get_tick(void)
{
    hrtime_t regs;
    asm volatile ("rpcc $0":"=A" (regs));	/* I'm not sure of syntax */
    return regs;
}
#else
#warning Unsupported CPU. Define function get_tick(). Disabling USE_XPROF_STATS...
#undef USE_XPROF_STATS
#endif

#endif /* USE_XPROF_STATS - maybe disabled above */

#ifdef USE_XPROF_STATS

typedef enum {
    XPROF_PROF_UNACCOUNTED,
    XPROF_PROF_OVERHEAD,
    XPROF_hash_lookup,
    XPROF_splay_splay,
    XPROF_xmalloc,
    XPROF_malloc,
    XPROF_xfree,
    XPROF_xxfree,
    XPROF_xrealloc,
    XPROF_xcalloc,
    XPROF_calloc,
    XPROF_xstrdup,
    XPROF_xstrndup,
    XPROF_xstrncpy,
    XPROF_xcountws,
    XPROF_memPoolChunkNew,
    XPROF_memPoolAlloc,
    XPROF_memPoolFree,
    XPROF_memPoolClean,
    XPROF_aclMatchAclList,
    XPROF_aclCheckFast,
    XPROF_comm_open,
    XPROF_comm_connect_addr,
    XPROF_comm_accept,
    XPROF_comm_close,
    XPROF_comm_udp_sendto,
    XPROF_commHandleWrite,
    XPROF_comm_check_incoming,
    XPROF_comm_poll_prep_pfds,
    XPROF_comm_poll_normal,
    XPROF_comm_handle_ready_fd,
    XPROF_comm_read_handler,
    XPROF_comm_write_handler,
    XPROF_storeGet,
    XPROF_storeMaintainSwapSpace,
    XPROF_storeRelease,
    XPROF_diskHandleWrite,
    XPROF_diskHandleRead,
    XPROF_file_open,
    XPROF_file_read,
    XPROF_file_write,
    XPROF_file_close,
    XPROF_LAST
} xprof_type;

#define XP_NOBEST 9999999999

typedef struct _xprof_stats_node xprof_stats_node;
typedef struct _xprof_stats_data xprof_stats_data;

struct _xprof_stats_data {
    hrtime_t start;
    hrtime_t stop;
    hrtime_t delta;
    hrtime_t best;
    hrtime_t worst;
    hrtime_t count;
    long long summ;
};

struct _xprof_stats_node {
    const char *name;
    xprof_stats_data accu;
    xprof_stats_data hist;
};

typedef xprof_stats_node TimersArray[1];

/* public Data */
extern TimersArray *xprof_Timers;
extern int xprof_nesting;

/* Exported functions */
extern void xprof_start(xprof_type type, const char *timer);
extern void xprof_stop(xprof_type type, const char *timer);
extern void xprof_event(void *data);

#define PROF_start(type) xprof_start(XPROF_##type, #type)
#define PROF_stop(type) xprof_stop(XPROF_##type, #type)

#else /* USE_XPROF_STATS */

#define PROF_start(ARGS) ((void)0)
#define PROF_stop(ARGS) ((void)0)

#endif /* USE_XPROF_STATS */

#endif /* _PROFILING_H_ */

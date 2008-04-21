
/*
 * $Id: MemPool.c 12352 2008-01-07 13:53:55Z hno $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../libcore/valgrind.h"
#include "../libcore/dlink.h"
#include "../libcore/debug.h"
#include "../libcore/tools.h"
#include "../libcore/gb.h"
#include "../include/Stack.h"
#include "../include/util.h"

#include "MemPool.h"

extern time_t squid_curtime;

/* exported */
unsigned int mem_pool_alloc_calls = 0;
unsigned int mem_pool_free_calls = 0;

/* module globals */

/* huge constant to set mem_idle_limit to "unlimited" */
static const size_t mem_unlimited_size = 2 * 1024 * MB - 1;

/* we cannot keep idle more than this limit */
size_t mem_idle_limit = 0;

/* memory pool accounting */
extern size_t mem_idle_limit;
MemPoolMeter TheMeter;
gb_t mem_traffic_volume = {0, 0};
Stack Pools;

struct {
	int do_zero;
	int limit;
	int enabled;
} MemPoolConfig = { 0, 0, 1 };

MemPoolStatInfo MemPoolStats = { 0, 0 };

/* local prototypes */
static void memShrink(size_t new_limit);
static void memPoolDescribe(const MemPool * pool);
static void memPoolShrink(MemPool * pool, size_t new_limit);

/* Initialization */

void
memConfigure(int enable, size_t limit, int dozero)
{
    size_t new_pool_limit = mem_idle_limit;
    /* set to configured value first */

    MemPoolConfig.enabled = enable;
    MemPoolConfig.limit = limit;
    MemPoolConfig.do_zero = dozero;
#if LEAK_CHECK_MODE
#if PURIFY
    if (1) {
#else
    if (RUNNING_ON_VALGRIND) {
#endif
	debug(63, 1) ("Disabling Memory pools for accurate leak checks\n");
	MemPoolConfig.enabled = 0;
    }
#endif
    if (! MemPoolConfig.enabled)
	new_pool_limit = 0;
    else if (MemPoolConfig.limit > 0)
	new_pool_limit = MemPoolConfig.limit;
    else
	new_pool_limit = mem_unlimited_size;
    /* shrink memory pools if needed */
    if (TheMeter.idle.level > new_pool_limit) {
	debug(63, 1) ("Shrinking idle mem pools to %.2f MB\n", toMB(new_pool_limit));
	memShrink(new_pool_limit);
    }
    assert(TheMeter.idle.level <= new_pool_limit);
    mem_idle_limit = new_pool_limit;
}

void
memInitModule(void)
{
    memset(&TheMeter, 0, sizeof(TheMeter));
    stackInit(&Pools);
    debug(63, 1) ("Memory pools are '%s'; limit: %.2f MB\n",
	(MemPoolConfig.enabled ? "on" : "off"), toMB(mem_idle_limit));
}

void
memCleanModule(void)
{
    int i;
    int dirty_count = 0;
    for (i = 0; i < Pools.count; i++) {
	MemPool *pool = Pools.items[i];
	if (!pool)
	    continue;
	if (memPoolInUseCount(pool)) {
	    memPoolDescribe(pool);
	    dirty_count++;
	} else {
	    memPoolDestroy(pool);
	}
    }
    if (dirty_count)
	debug(63, 2) ("memCleanModule: %d pools are left dirty\n", dirty_count);
    /* we clean the stack anyway */
    stackClean(&Pools);
}


static void
memShrink(size_t new_limit)
{
    size_t start_limit = TheMeter.idle.level;
    int i;
    debug(63, 1) ("memShrink: started with %ld KB goal: %ld KB\n",
	(long int) toKB(TheMeter.idle.level), (long int) toKB(new_limit));
    /* first phase: cut proportionally to the pool idle size */
    for (i = 0; i < Pools.count && TheMeter.idle.level > new_limit; ++i) {
	MemPool *pool = Pools.items[i];
	const size_t target_pool_size = (size_t) ((double) pool->meter.idle.level * new_limit) / start_limit;
	memPoolShrink(pool, target_pool_size);
    }
    debug(63, 1) ("memShrink: 1st phase done with %ld KB left\n", (long int) toKB(TheMeter.idle.level));
    /* second phase: cut to 0 */
    for (i = 0; i < Pools.count && TheMeter.idle.level > new_limit; ++i)
	memPoolShrink(Pools.items[i], 0);
    debug(63, 1) ("memShrink: 2nd phase done with %ld KB left\n", (long int) toKB(TheMeter.idle.level));
    assert(TheMeter.idle.level <= new_limit);	/* paranoid */
}

/* MemPoolMeter */

/* MemMeter */

void
memMeterSyncHWater(MemMeter * m)
{
    assert(m);
    if (m->hwater_level < m->level) {
	m->hwater_level = m->level;
	m->hwater_stamp = squid_curtime;
    }
}

/* MemPool */

MemPool *
memPoolCreate(const char *label, size_t obj_size)
{
    MemPool *pool = xcalloc(1, sizeof(MemPool));
    assert(label && obj_size);
    pool->label = label;
    pool->obj_size = obj_size;
#if DEBUG_MEMPOOL
    pool->real_obj_size = (obj_size & 7) ? (obj_size | 7) + 1 : obj_size;
#endif
    pool->flags.dozero = 1;
    stackInit(&pool->pstack);
    /* other members are set to 0 */
    stackPush(&Pools, pool);
    return pool;
}

void
memPoolNonZero(MemPool * p)
{
    p->flags.dozero = 0;
}

void
memPoolDestroy(MemPool * pool)
{
    int i;
    assert(pool);
    for (i = 0; i < Pools.count; i++) {
	if (Pools.items[i] == pool) {
	    Pools.items[i] = NULL;
	    break;
	}
    }
    stackClean(&pool->pstack);
    xfree(pool);
}

#if DEBUG_MEMPOOL
#define MEMPOOL_COOKIE(p) ((void *)((unsigned long)(p) ^ 0xDEADBEEF))
#define MEMPOOL_COOKIE2(p) ((void *)((unsigned long)(p) ^ 0xFEEDBEEF))
struct mempool_cookie {
    MemPool *pool;
    void *cookie;
};

#endif

void *
memPoolAlloc(MemPool * pool)
{
    void *obj;
    assert(pool);
    memMeterInc(pool->meter.inuse);
    gb_inc(&pool->meter.total, 1);
    gb_inc(&TheMeter.total, pool->obj_size);
    memMeterAdd(TheMeter.inuse, pool->obj_size);
    gb_inc(&mem_traffic_volume, pool->obj_size);
    mem_pool_alloc_calls++;
    if (pool->pstack.count) {
	assert(pool->meter.idle.level);
	memMeterDec(pool->meter.idle);
	memMeterDel(TheMeter.idle, pool->obj_size);
	gb_inc(&pool->meter.saved, 1);
	gb_inc(&TheMeter.saved, pool->obj_size);
	obj = stackPop(&pool->pstack);
#if DEBUG_MEMPOOL
	(void) VALGRIND_MAKE_MEM_DEFINED(obj, pool->real_obj_size + sizeof(struct mempool_cookie));
#else
	(void) VALGRIND_MAKE_MEM_DEFINED(obj, pool->obj_size);
#endif
#if DEBUG_MEMPOOL
	{
	    struct mempool_cookie *cookie = (void *) (((unsigned char *) obj) + pool->real_obj_size);
	    assert(cookie->cookie == MEMPOOL_COOKIE2(obj));
	    assert(cookie->pool == pool);
	    cookie->cookie = MEMPOOL_COOKIE(obj);
	    (void) VALGRIND_MAKE_MEM_NOACCESS(cookie, sizeof(cookie));
	}
	if (Config.onoff.zero_buffers || pool->flags.dozero)
	    memset(obj, 0, pool->obj_size);
#endif
    } else {
	assert(!pool->meter.idle.level);
	memMeterInc(pool->meter.alloc);
	memMeterAdd(TheMeter.alloc, pool->obj_size);
#if DEBUG_MEMPOOL
	{
	    struct mempool_cookie *cookie;
	    obj = xcalloc(1, pool->real_obj_size + sizeof(struct mempool_cookie));
	    cookie = (struct mempool_cookie *) (((unsigned char *) obj) + pool->real_obj_size);
	    cookie->cookie = MEMPOOL_COOKIE(obj);
	    cookie->pool = pool;
	    (void) VALGRIND_MAKE_MEM_NOACCESS(cookie, sizeof(cookie));
	}
#else
	if (MemPoolConfig.do_zero || pool->flags.dozero)
	    obj = xcalloc(1, pool->obj_size);
	else
	    obj = xmalloc(pool->obj_size);
#endif
    }
    return obj;
}

void
memPoolFree(MemPool * pool, void *obj)
{
    assert(pool && obj);
    memMeterDec(pool->meter.inuse);
    memMeterDel(TheMeter.inuse, pool->obj_size);
    mem_pool_free_calls++;
    (void) VALGRIND_CHECK_MEM_IS_ADDRESSABLE(obj, pool->obj_size);
#if DEBUG_MEMPOOL
    {
	struct mempool_cookie *cookie = (void *) (((unsigned char *) obj) + pool->real_obj_size);
	(void) VALGRIND_MAKE_MEM_DEFINED(cookie, sizeof(cookie));
	assert(cookie->cookie == MEMPOOL_COOKIE(obj));
	assert(cookie->pool == pool);
	cookie->cookie = MEMPOOL_COOKIE2(obj);
    }
#endif
    if (TheMeter.idle.level + pool->obj_size <= mem_idle_limit) {
	memMeterInc(pool->meter.idle);
	memMeterAdd(TheMeter.idle, pool->obj_size);
	if (MemPoolConfig.do_zero || pool->flags.dozero)
#if DEBUG_MEMPOOL
	    memset(obj, 0xf0, pool->obj_size);
	(void) VALGRIND_MAKE_MEM_NOACCESS(obj, pool->real_obj_size + sizeof(struct mempool_cookie));
#else
	    memset(obj, 0, pool->obj_size);
	(void) VALGRIND_MAKE_MEM_NOACCESS(obj, pool->obj_size);
#endif
	stackPush(&pool->pstack, obj);
    } else {
	memMeterDec(pool->meter.alloc);
	memMeterDel(TheMeter.alloc, pool->obj_size);
	xfree(obj);
    }
    assert(pool->meter.idle.level <= pool->meter.alloc.level);
}

static void
memPoolShrink(MemPool * pool, size_t new_limit)
{
    assert(pool);
    while (pool->meter.idle.level > new_limit && pool->pstack.count > 0) {
	memMeterDec(pool->meter.alloc);
	memMeterDec(pool->meter.idle);
	memMeterDel(TheMeter.idle, pool->obj_size);
	memMeterDel(TheMeter.alloc, pool->obj_size);
	xfree(stackPop(&pool->pstack));
    }
    assert(pool->meter.idle.level <= new_limit);	/* paranoid */
}

int
memPoolWasUsed(const MemPool * pool)
{
    assert(pool);
    return pool->meter.alloc.hwater_level > 0;
}

int
memPoolInUseCount(const MemPool * pool)
{
    assert(pool);
    return pool->meter.inuse.level;
}

size_t
memPoolInUseSize(const MemPool * pool)
{
    assert(pool);
    return (pool->obj_size * pool->meter.inuse.level);
}

/* to-do: make debug level a parameter? */
static void
memPoolDescribe(const MemPool * pool)
{
    assert(pool);
    debug(63, 2) ("%-20s: %6d x %4d bytes = %5ld KB\n",
	pool->label, memPoolInUseCount(pool), (int) pool->obj_size,
	(long int) toKB(memPoolInUseSize(pool)));
}

size_t
memTotalAllocated(void)
{
    return TheMeter.alloc.level;
}


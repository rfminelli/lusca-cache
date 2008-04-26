
/*
 * $Id$
 *
 * DEBUG: section 13    High Level Memory Pool Management
 * AUTHOR: Harvest Derived
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *
 */

#include "squid.h"

/* module globals */

static MemPool *MemPools[MEM_MAX];

/* string pools */

static MemMeter HugeBufCountMeter;
static MemMeter HugeBufVolumeMeter;

/* local routines */

static void
memStringStats(StoreEntry * sentry)
{
    const char *pfmt = "%-20s\t %d\t %d\n";
    int i;
    int pooled_count = 0;
    size_t pooled_volume = 0;
    /* heading */
    storeAppendPrintf(sentry,
	"String Pool\t Impact\t\t\n"
	" \t (%%strings)\t (%%volume)\n");
    /* table body */
    for (i = 0; i < MEM_STR_POOL_COUNT; i++) {
	const MemPool *pool = StrPools[i].pool;
	const int plevel = pool->meter.inuse.level;
	storeAppendPrintf(sentry, pfmt,
	    pool->label,
	    xpercentInt(plevel, StrCountMeter.level),
	    xpercentInt(plevel * pool->obj_size, StrVolumeMeter.level));
	pooled_count += plevel;
	pooled_volume += plevel * pool->obj_size;
    }
    /* malloc strings */
    storeAppendPrintf(sentry, pfmt,
	"Other Strings",
	xpercentInt(StrCountMeter.level - pooled_count, StrCountMeter.level),
	xpercentInt(StrVolumeMeter.level - pooled_volume, StrVolumeMeter.level));
    storeAppendPrintf(sentry, "\n");
}

static void
memBufStats(StoreEntry * sentry)
{
    storeAppendPrintf(sentry, "Large buffers: %d (%d KB)\n",
	(int) HugeBufCountMeter.level,
	(int) HugeBufVolumeMeter.level / 1024);
}

static void
memStats(StoreEntry * sentry)
{
    storeBuffer(sentry);
    memReport(sentry);
    memStringStats(sentry);
    memBufStats(sentry);
    storeBufferFlush(sentry);
#if WITH_VALGRIND
    if (RUNNING_ON_VALGRIND) {
	long int leaked = 0, dubious = 0, reachable = 0, suppressed = 0;
	storeAppendPrintf(sentry, "Valgrind Report:\n");
	storeAppendPrintf(sentry, "Type\tAmount\n");
	debug(13, 1) ("Asking valgrind for memleaks\n");
	VALGRIND_DO_LEAK_CHECK;
	debug(13, 1) ("Getting valgrind statistics\n");
	VALGRIND_COUNT_LEAKS(leaked, dubious, reachable, suppressed);
	storeAppendPrintf(sentry, "Leaked\t%ld\n", leaked);
	storeAppendPrintf(sentry, "Dubious\t%ld\n", dubious);
	storeAppendPrintf(sentry, "Reachable\t%ld\n", reachable);
	storeAppendPrintf(sentry, "Suppressed\t%ld\n", suppressed);
    }
#endif
}


/*
 * public routines
 */

/*
 * we have a limit on _total_ amount of idle memory so we ignore
 * max_pages for now
 */
void
memDataInit(mem_type type, const char *name, size_t size, int max_pages_notused)
{
    assert(name && size);
    MemPools[type] = memPoolCreate(name, size);
}

void
memDataNonZero(mem_type type)
{
    memPoolNonZero(MemPools[type]);
}


/* find appropriate pool and use it (pools always init buffer with 0s) */
void *
memAllocate(mem_type type)
{
    return memPoolAlloc(MemPools[type]);
}

/* give memory back to the pool */
void
memFree(void *p, int type)
{
    memPoolFree(MemPools[type], p);
}

/* Find the best fit MEM_X_BUF type */
static mem_type
memFindBufSizeType(size_t net_size, size_t * gross_size)
{
    mem_type type;
    size_t size;
    if (net_size <= 2 * 1024) {
	type = MEM_2K_BUF;
	size = 2 * 1024;
    } else if (net_size <= 4 * 1024) {
	type = MEM_4K_BUF;
	size = 4 * 1024;
    } else if (net_size <= 8 * 1024) {
	type = MEM_8K_BUF;
	size = 8 * 1024;
    } else if (net_size <= 16 * 1024) {
	type = MEM_16K_BUF;
	size = 16 * 1024;
    } else if (net_size <= 32 * 1024) {
	type = MEM_32K_BUF;
	size = 32 * 1024;
    } else if (net_size <= 64 * 1024) {
	type = MEM_64K_BUF;
	size = 64 * 1024;
    } else {
	type = MEM_NONE;
	size = net_size;
    }
    if (gross_size)
	*gross_size = size;
    return type;
}

/* allocate a variable size buffer using best-fit pool */
void *
memAllocBuf(size_t net_size, size_t * gross_size)
{
    mem_type type = memFindBufSizeType(net_size, gross_size);
    if (type != MEM_NONE)
	return memAllocate(type);
    else {
	memMeterInc(HugeBufCountMeter);
	memMeterAdd(HugeBufVolumeMeter, *gross_size);
	return xcalloc(1, net_size);
    }
}

/* resize a variable sized buffer using best-fit pool */
void *
memReallocBuf(void *oldbuf, size_t net_size, size_t * gross_size)
{
    /* XXX This can be optimized on very large buffers to use realloc() */
    size_t new_gross_size;
    void *newbuf = memAllocBuf(net_size, &new_gross_size);
    if (oldbuf) {
	int data_size = *gross_size;
	if (data_size > net_size)
	    data_size = net_size;
	memcpy(newbuf, oldbuf, data_size);
	memFreeBuf(*gross_size, oldbuf);
    }
    *gross_size = new_gross_size;
    return newbuf;
}

/* free buffer allocated with memAllocBuf() */
void
memFreeBuf(size_t size, void *buf)
{
    mem_type type = memFindBufSizeType(size, NULL);
    if (type != MEM_NONE)
	memFree(buf, type);
    else {
	xfree(buf);
	memMeterDec(HugeBufCountMeter);
	memMeterDel(HugeBufVolumeMeter, size);
    }
}
void
memBuffersInit()
{
    memDataInit(MEM_2K_BUF, "2K Buffer", 2048, 10);
    memDataNonZero(MEM_2K_BUF);
    memDataInit(MEM_4K_BUF, "4K Buffer", 4096, 10);
    memDataNonZero(MEM_4K_BUF);
    memDataInit(MEM_8K_BUF, "8K Buffer", 8192, 10);
    memDataNonZero(MEM_8K_BUF);
    memDataInit(MEM_16K_BUF, "16K Buffer", 16384, 10);
    memDataNonZero(MEM_16K_BUF);
    memDataInit(MEM_32K_BUF, "32K Buffer", 32768, 10);
    memDataNonZero(MEM_32K_BUF);
    memDataInit(MEM_64K_BUF, "64K Buffer", 65536, 10);
    memDataNonZero(MEM_64K_BUF);
}


void
memInit(void)
{
    memInitModule();
    /* set all pointers to null */
    memset(MemPools, '\0', sizeof(MemPools));
    /*
     * it does not hurt much to have a lot of pools since sizeof(MemPool) is
     * small; someday we will figure out what to do with all the entries here
     * that are never used or used only once; perhaps we should simply use
     * malloc() for those? @?@
     */
    aclInitMem();
    memBuffersInit();
    authenticateInitMem();
#if USE_CACHE_DIGESTS
    peerDigestInitMem();
#endif
    disk_init_mem();
    fwdInitMem();
    httpHeaderInitMem();
    stmemInitMem();
    storeInitMem();
    netdbInitMem();
    requestInitMem();
    helperInitMem();
    /* Those below require conversion */
    cacheCfInitMem();
    clientdbInitMem();
    storeSwapTLVInitMem();
    cachemgrRegister("mem",
	"Memory Utilization",
	memStats, 0, 1);
}

/*
 * Test that all entries are initialized
 */
void
memCheckInit(void)
{
    mem_type t;
    for (t = MEM_NONE, t++; t < MEM_MAX; t++) {
	if (MEM_DONTFREE == t)
	    continue;
	/*
	 * If you hit this assertion, then you forgot to add a
	 * memDataInit() line for type 't'.
	 */
	assert(MemPools[t]);
    }
}

void
memClean(void)
{
    memCleanModule();
}

int
memInUse(mem_type type)
{
    return memPoolInUseCount(MemPools[type]);
}

/* ick */

static void
memFree2K(void *p)
{
    memFree(p, MEM_2K_BUF);
}

void
memFree4K(void *p)
{
    memFree(p, MEM_4K_BUF);
}

void
memFree8K(void *p)
{
    memFree(p, MEM_8K_BUF);
}

static void
memFree16K(void *p)
{
    memFree(p, MEM_16K_BUF);
}

static void
memFree32K(void *p)
{
    memFree(p, MEM_32K_BUF);
}

static void
memFree64K(void *p)
{
    memFree(p, MEM_64K_BUF);
}

FREE *
memFreeBufFunc(size_t size)
{
    switch (size) {
    case 2 * 1024:
	return memFree2K;
    case 4 * 1024:
	return memFree4K;
    case 8 * 1024:
	return memFree8K;
    case 16 * 1024:
	return memFree16K;
    case 32 * 1024:
	return memFree32K;
    case 64 * 1024:
	return memFree64K;
    default:
	memMeterDec(HugeBufCountMeter);
	memMeterDel(HugeBufVolumeMeter, size);
	return xfree;
    }
}


/*
 * $Id$
 *
 * DEBUG: section 77    Delay Pools
 * AUTHOR: David Luyer <luyer@ucs.uwa.edu.au>
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by the
 *  National Science Foundation.  Squid is Copyrighted (C) 1998 by
 *  Duane Wessels and the University of California San Diego.  Please
 *  see the COPYRIGHT file for full details.  Squid incorporates
 *  software developed and/or copyrighted by other sources.  Please see
 *  the CREDITS file for full details.
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

#include "config.h"

#if DELAY_POOLS
#include "squid.h"

struct _class1DelayPool {
    int aggregate;
};

struct _class2DelayPool {
    int aggregate;
    /* OK: -1 is terminator.  individual[255] is always host 255. */
    /* 255 entries + 1 terminator byte */
    unsigned char individual_map[256];
    unsigned char individual_255_used;
    /* 256 entries */
    int individual[256];
};

struct _class3DelayPool {
    int aggregate;
    /* OK: -1 is terminator.  network[255] is always host 255. */
    /* 255 entries + 1 terminator byte */
    unsigned char network_map[256];
    unsigned char network_255_used;
    /* 256 entries */
    int network[256];
    /* 256 sets of (255 entries + 1 terminator byte) */
    unsigned char individual_map[256][256];
    /* Pack this into one bit per net */
    unsigned char individual_255_used[32];
    /* largest entry = (255<<8)+255 = 65535 */
    int individual[65536];
};

typedef struct _class1DelayPool class1DelayPool;
typedef struct _class2DelayPool class2DelayPool;
typedef struct _class3DelayPool class3DelayPool;

union _delayPool {
    class1DelayPool *class1;
    class2DelayPool *class2;
    class3DelayPool *class3;
};

typedef union _delayPool delayPool;

static delayPool *delay_data = NULL;
static fd_set delay_no_delay;
static time_t delay_pools_last_update = 0;
static hash_table *delay_id_ptr_hash = NULL;

static OBJH delayPoolStats;

static unsigned int
delayIdPtrHash(const void *key, unsigned int n)
{
    /* Hashes actual POINTER VALUE.
     * Assumes <= 256 hash buckets & even hash size.
     * Assumes the most variation in pointers to inside
     * medium size objects occurs in the 2nd and 3rd
     * least significant bytes.
     */
    const char *ptr = (char *) &key;
#if SIZEOF_VOID_P == 4
    return (ptr[1] ^ ptr[2]) & (n - 1);
#elif SIZEOF_VOID_P == 8
#if WORDS_BIGENDIAN
    return (ptr[5] ^ ptr[6]) & (n - 1);
#else
    return (ptr[1] ^ ptr[2]) & (n - 1);
#endif
#else
#error What kind of a sick architecture are you on anyway?
#endif
}

static int
delayIdPtrHashCmp(const void *a, const void *b)
{
    /* Sort by POINTER VALUE. */
    return b - a;
}

void
delayPoolsInit(void)
{
    delay_pools_last_update = getCurrentTime();
    FD_ZERO(&delay_no_delay);
    cachemgrRegister("delay", "Delay Pool Levels", delayPoolStats, 0, 1);
}

void
delayInitDelayData(unsigned short pools)
{
    if (pools) {
	delay_data = xcalloc(pools, sizeof(delayPool));
	eventAdd("delayPoolsUpdate", delayPoolsUpdate, NULL, 1.0, 1);
	delay_id_ptr_hash = hash_create(delayIdPtrHashCmp, 256, delayIdPtrHash);
    }
}

static void
delayIdZero(void *hlink)
{
    hash_link *h = hlink;
    *(delay_id *) (h->key) = 0;
    xfree(h);
}

void
delayFreeDelayData()
{
    safe_free(delay_data);
    if (delay_id_ptr_hash) {
	hashFreeItems(delay_id_ptr_hash, delayIdZero);
	hashFreeMemory(delay_id_ptr_hash);
	delay_id_ptr_hash = NULL;
    }
}

void
delayRegisterDelayIdPtr(delay_id * loc)
{
    hash_link *lnk = xmalloc(sizeof(hash_link));

    lnk->key = (char *) loc;
    hash_join(delay_id_ptr_hash, lnk);
}

void
delayUnregisterDelayIdPtr(delay_id * loc)
{
    hash_link *lnk = hash_lookup(delay_id_ptr_hash, loc);

    assert(lnk);
    hash_remove_link(delay_id_ptr_hash, lnk);
    xxfree(lnk);
}

void
delayCreateDelayPool(unsigned short pool, u_char class)
{
    switch (class) {
    case 1:
	delay_data[pool].class1 = xmalloc(sizeof(class1DelayPool));
	break;
    case 2:
	delay_data[pool].class2 = xmalloc(sizeof(class2DelayPool));
	break;
    case 3:
	delay_data[pool].class3 = xmalloc(sizeof(class3DelayPool));
	break;
    default:
	assert(0);
    }
}

void
delayInitDelayPool(unsigned short pool, u_char class, delaySpecSet * rates)
{
    /* delaySetSpec may be pointer to partial structure so MUST pass by
     * reference.
     */
    switch (class) {
    case 1:
	delay_data[pool].class1->aggregate = (rates->aggregate.max_bytes *
	    Config.Delay.initial) / 100;
	break;
    case 2:
	delay_data[pool].class2->aggregate = (rates->aggregate.max_bytes *
	    Config.Delay.initial) / 100;
	delay_data[pool].class2->individual_map[0] = 255;
	delay_data[pool].class2->individual_255_used = 0;
	break;
    case 3:
	delay_data[pool].class3->aggregate = (rates->aggregate.max_bytes *
	    Config.Delay.initial) / 100;
	delay_data[pool].class3->network_map[0] = 255;
	delay_data[pool].class3->network_255_used = 0;
	memset(&delay_data[pool].class3->individual_255_used, '\0',
	    sizeof(delay_data[pool].class3->individual_255_used));
	break;
    default:
	assert(0);
    }
}

void
delayFreeDelayPool(unsigned short pool)
{
    /* this is a union - and all free() cares about is the pointer location */
    safe_free(delay_data[pool].class1);
}

void
delaySetNoDelay(int fd)
{
    FD_SET(fd, &delay_no_delay);
}

void
delayClearNoDelay(int fd)
{
    FD_CLR(fd, &delay_no_delay);
}

int
delayIsNoDelay(int fd)
{
    return FD_ISSET(fd, &delay_no_delay);
}

static delay_id
delayId(unsigned short pool, unsigned short position)
{
    return (pool << 16) | position;
}

delay_id
delayClient(request_t * r)
{
    aclCheck_t ch;
    int i;
    int j;
    unsigned int host;
    unsigned short pool, position;
    unsigned char class, net;

    memset(&ch, '\0', sizeof(ch));
    ch.src_addr = r->client_addr;
    ch.my_addr = r->my_addr;
    ch.request = r;
    for (pool = 0; pool < Config.Delay.pools; pool++) {
	if (aclCheckFast(Config.Delay.access[pool], &ch))
	    break;
    }
    if (pool == Config.Delay.pools)
	return delayId(0, 0);
    class = Config.Delay.class[pool];
    if (class == 0)
	return delayId(0, 0);
    if (class == 1)
	return delayId(pool + 1, 0);
    if (class == 2) {
	host = ntohl(ch.src_addr.s_addr) & 0xff;
	if (host == 255) {
	    if (!delay_data[pool].class2->individual_255_used) {
		delay_data[pool].class2->individual_255_used = 1;
		delay_data[pool].class2->individual[255] =
		    (Config.Delay.rates[pool]->individual.max_bytes *
		    Config.Delay.initial) / 100;
	    }
	    return delayId(pool + 1, 255);
	}
	for (i = 0;; i++) {
	    if (delay_data[pool].class2->individual_map[i] == host)
		break;
	    if (delay_data[pool].class2->individual_map[i] == 255) {
		delay_data[pool].class2->individual_map[i] = host;
		delay_data[pool].class2->individual_map[i + 1] = 255;
		delay_data[pool].class2->individual[i] =
		    (Config.Delay.rates[pool]->individual.max_bytes *
		    Config.Delay.initial) / 100;
		break;
	    }
	}
	return delayId(pool + 1, i);
    }
    /* class == 3 */
    host = ntohl(ch.src_addr.s_addr) & 0xffff;
    net = host >> 8;
    host &= 0xff;
    if (net == 255) {
	i = 255;
	if (!delay_data[pool].class3->network_255_used) {
	    delay_data[pool].class3->network_255_used = 1;
	    delay_data[pool].class3->network[255] =
		(Config.Delay.rates[pool]->network.max_bytes *
		Config.Delay.initial) / 100;
	}
    } else {
	for (i = 0;; i++) {
	    if (delay_data[pool].class3->network_map[i] == net)
		break;
	    if (delay_data[pool].class3->network_map[i] == 255) {
		delay_data[pool].class3->network_map[i] = net;
		delay_data[pool].class3->individual_map[i][0] = 255;
		delay_data[pool].class3->network_map[i + 1] = 255;
		delay_data[pool].class3->network[i] =
		    (Config.Delay.rates[pool]->network.max_bytes *
		    Config.Delay.initial) / 100;
		break;
	    }
	}
    }
    position = i << 8;
    if (host == 255) {
	position |= 255;
	if (!(delay_data[pool].class3->individual_255_used[i / 8] & (1 << (i % 8)))) {
	    delay_data[pool].class3->individual_255_used[i / 8] |= (1 << (i % 8));
	    delay_data[pool].class3->individual[position] =
		(Config.Delay.rates[pool]->individual.max_bytes *
		Config.Delay.initial) / 100;
	}
	return delayId(pool + 1, position);
    }
    for (j = 0;; j++) {
	if (delay_data[pool].class3->individual_map[i][j] == host) {
	    position |= j;
	    break;
	}
	if (delay_data[pool].class3->individual_map[i][j] == 255) {
	    delay_data[pool].class3->individual_map[i][j] = host;
	    delay_data[pool].class3->individual_map[i][j + 1] = 255;
	    delay_data[pool].class3->individual[position |= j] =
		(Config.Delay.rates[pool]->individual.max_bytes *
		Config.Delay.initial) / 100;
	    break;
	}
    }
    return delayId(pool + 1, position);
}

static void
delayUpdateClass1(class1DelayPool * class1, delaySpecSet * rates, int incr)
{
    /* delaySetSpec may be pointer to partial structure so MUST pass by
     * reference.
     */
    if (rates->aggregate.restore_bps != -1 &&
	(class1->aggregate += rates->aggregate.restore_bps * incr) >
	rates->aggregate.max_bytes)
	class1->aggregate = rates->aggregate.max_bytes;
}

static void
delayUpdateClass2(class2DelayPool * class2, delaySpecSet * rates, int incr)
{
    int restore_bytes;
    unsigned char i;
    /* delaySetSpec may be pointer to partial structure so MUST pass by
     * reference.
     */
    if (rates->aggregate.restore_bps != -1 &&
	(class2->aggregate += rates->aggregate.restore_bps * incr) >
	rates->aggregate.max_bytes)
	class2->aggregate = rates->aggregate.max_bytes;
    if ((restore_bytes = rates->individual.restore_bps) == -1)
	return;
    restore_bytes *= incr;
    if (class2->individual_255_used)
	i = 255;
    else
	i = 0;
    for (;;) {
	if (i != 255 && class2->individual_map[i] == 255)
	    return;
	if (class2->individual[i] != rates->individual.max_bytes &&
	    (class2->individual[i] += restore_bytes) > rates->individual.max_bytes)
	    class2->individual[i] = rates->individual.max_bytes;
	if (++i == 255)
	    return;
    }
}

static void
delayUpdateClass3(class3DelayPool * class3, delaySpecSet * rates, int incr)
{
    int individual_restore_bytes, network_restore_bytes;
    int mpos;
    unsigned char i, j;
    /* delaySetSpec may be pointer to partial structure so MUST pass by
     * reference.
     */
    if (rates->aggregate.restore_bps != -1 &&
	(class3->aggregate += rates->aggregate.restore_bps * incr) >
	rates->aggregate.max_bytes)
	class3->aggregate = rates->aggregate.max_bytes;
    /* the following line deliberately uses &, not &&, in an if statement
     * to avoid conditional execution
     */
    if (((network_restore_bytes = rates->network.restore_bps) == -1) &
	((individual_restore_bytes = rates->individual.restore_bps) == -1))
	return;
    individual_restore_bytes *= incr;
    network_restore_bytes *= incr;
    if (class3->network_255_used)
	i = 255;
    else
	i = 0;
    for (;;) {
	if (i != 255 && class3->network_map[i] == 255)
	    return;
	if (individual_restore_bytes != -incr) {
	    mpos = i << 8;
	    if (class3->individual_255_used[i / 8] & (1 << (i % 8)))
		j = 255;
	    else
		j = 0;
	    for (;;) {
		if (j != 255 && class3->individual_map[i][j] == 255)
		    break;
		if (class3->individual[mpos] != rates->individual.max_bytes &&
		    (class3->individual[mpos] += individual_restore_bytes) >
		    rates->individual.max_bytes)
		    class3->individual[mpos] = rates->individual.max_bytes;
		mpos++;
		if (j == 255)
		    mpos -= 256;
		if (++j == 255)
		    break;
	    }
	}
	if (network_restore_bytes != -incr &&
	    class3->network[i] != rates->network.max_bytes &&
	    (class3->network[i] += network_restore_bytes) >
	    rates->network.max_bytes)
	    class3->network[i] = rates->network.max_bytes;
	if (++i == 255)
	    return;
    }
}

void
delayPoolsUpdate(void *unused)
{
    int incr = squid_curtime - delay_pools_last_update;
    unsigned short i;
    unsigned char class;
    if (!Config.Delay.pools)
	return;
    eventAdd("delayPoolsUpdate", delayPoolsUpdate, NULL, 1.0, 1);
    if (incr < 1)
	return;
    delay_pools_last_update = squid_curtime;
    for (i = 0; i < Config.Delay.pools; i++) {
	class = Config.Delay.class[i];
	if (!class)
	    continue;
	switch (class) {
	case 1:
	    delayUpdateClass1(delay_data[i].class1, Config.Delay.rates[i], incr);
	    break;
	case 2:
	    delayUpdateClass2(delay_data[i].class2, Config.Delay.rates[i], incr);
	    break;
	case 3:
	    delayUpdateClass3(delay_data[i].class3, Config.Delay.rates[i], incr);
	    break;
	default:
	    assert(0);
	}
    }
}

/*
 * this returns the number of bytes the client is permitted. it does not take
 * into account bytes already buffered - that is up to the caller.
 */
int
delayBytesWanted(delay_id d, int min, int max)
{
    unsigned short position = d & 0xFFFF;
    unsigned short pool = (d >> 16) - 1;
    unsigned char class = (pool == 0xFFFF) ? 0 : Config.Delay.class[pool];
    int nbytes = max;

    switch (class) {
    case 0:
	break;

    case 1:
	if (Config.Delay.rates[pool]->aggregate.restore_bps != -1)
	    nbytes = XMIN(nbytes, delay_data[pool].class1->aggregate);
	break;

    case 2:
	if (Config.Delay.rates[pool]->aggregate.restore_bps != -1)
	    nbytes = XMIN(nbytes, delay_data[pool].class2->aggregate);
	if (Config.Delay.rates[pool]->individual.restore_bps != -1)
	    nbytes = XMIN(nbytes, delay_data[pool].class2->individual[position]);
	break;

    case 3:
	if (Config.Delay.rates[pool]->aggregate.restore_bps != -1)
	    nbytes = XMIN(nbytes, delay_data[pool].class3->aggregate);
	if (Config.Delay.rates[pool]->individual.restore_bps != -1)
	    nbytes = XMIN(nbytes, delay_data[pool].class3->individual[position]);
	if (Config.Delay.rates[pool]->network.restore_bps != -1)
	    nbytes = XMIN(nbytes, delay_data[pool].class3->network[position >> 8]);
	break;

    default:
	fatalf("delayBytesWanted: Invalid class %d\n", class);
	break;
    }
    nbytes = XMAX(min, nbytes);
    return nbytes;
}

/*
 * this records actual bytes recieved.  always recorded, even if the
 * class is disabled - it's more efficient to just do it than to do all
 * the checks.
 */
void
delayBytesIn(delay_id d, int qty)
{
    unsigned short position = d & 0xFFFF;
    unsigned short pool = (d >> 16) - 1;
    unsigned char class;

    if (pool == 0xFFFF)
	return;
    class = Config.Delay.class[pool];
    switch (class) {
    case 1:
	delay_data[pool].class1->aggregate -= qty;
	return;
    case 2:
	delay_data[pool].class2->aggregate -= qty;
	delay_data[pool].class2->individual[position] -= qty;
	return;
    case 3:
	delay_data[pool].class3->aggregate -= qty;
	delay_data[pool].class3->network[position >> 8] -= qty;
	delay_data[pool].class3->individual[position] -= qty;
	return;
    }
    fatalf("delayBytesWanted: Invalid class %d\n", class);
    assert(0);
}

int
delayMostBytesWanted(const MemObject * mem, int max)
{
    int i = 0;
    int found = 0;
    store_client *sc;
    for (sc = mem->clients; sc; sc = sc->next) {
	if (sc->callback_data == NULL)	/* open slot */
	    continue;
	if (sc->type != STORE_MEM_CLIENT)
	    continue;
	i = delayBytesWanted(sc->delay_id, i, max);
	found = 1;
    }
    return found ? i : max;
}

delay_id
delayMostBytesAllowed(const MemObject * mem)
{
    int j;
    int jmax = -1;
    store_client *sc;
    delay_id d = 0;
    for (sc = mem->clients; sc; sc = sc->next) {
	if (sc->callback_data == NULL)	/* open slot */
	    continue;
	if (sc->type != STORE_MEM_CLIENT)
	    continue;
	j = delayBytesWanted(sc->delay_id, 0, SQUID_TCP_SO_RCVBUF);
	if (j > jmax) {
	    jmax = j;
	    d = sc->delay_id;
	}
    }
    return d;
}

void
delaySetStoreClient(StoreEntry * e, void *data, delay_id delay_id)
{
    store_client *sc = storeClientListSearch(e->mem_obj, data);
    assert(sc != NULL);
    sc->delay_id = delay_id;
    delayRegisterDelayIdPtr(&sc->delay_id);
}

static void
delayPoolStatsAg(StoreEntry * sentry, delaySpecSet * rate, int ag)
{
    /* note - always pass delaySpecSet's by reference as may be incomplete */
    if (rate->aggregate.restore_bps == -1) {
	storeAppendPrintf(sentry, "\tAggregate:\n\t\tDisabled.\n\n");
	return;
    }
    storeAppendPrintf(sentry, "\tAggregate:\n");
    storeAppendPrintf(sentry, "\t\tMax: %d\n", rate->aggregate.max_bytes);
    storeAppendPrintf(sentry, "\t\tRestore: %d\n", rate->aggregate.restore_bps);
    storeAppendPrintf(sentry, "\t\tCurrent: %d\n\n", ag);
}

static void
delayPoolStats1(StoreEntry * sentry, unsigned short pool)
{
    /* must be a reference only - partially malloc()d struct */
    delaySpecSet *rate = Config.Delay.rates[pool];

    storeAppendPrintf(sentry, "Pool: %d\n\tClass: 1\n\n", pool + 1);
    delayPoolStatsAg(sentry, rate, delay_data[pool].class1->aggregate);
}

static void
delayPoolStats2(StoreEntry * sentry, unsigned short pool)
{
    /* must be a reference only - partially malloc()d struct */
    delaySpecSet *rate = Config.Delay.rates[pool];
    class2DelayPool *class2 = delay_data[pool].class2;
    unsigned char shown = 0, i;

    storeAppendPrintf(sentry, "Pool: %d\n\tClass: 2\n\n", pool + 1);
    delayPoolStatsAg(sentry, rate, class2->aggregate);
    if (rate->individual.restore_bps == -1) {
	storeAppendPrintf(sentry, "\tIndividual:\n\t\tDisabled.\n\n");
	return;
    }
    storeAppendPrintf(sentry, "\tIndividual:\n");
    storeAppendPrintf(sentry, "\t\tMax: %d\n", rate->individual.max_bytes);
    storeAppendPrintf(sentry, "\t\tRate: %d\n", rate->individual.restore_bps);
    storeAppendPrintf(sentry, "\t\tCurrent: ");
    for (i = 0;; i++) {
	if (class2->individual_map[i] == 255)
	    break;
	storeAppendPrintf(sentry, "%d:%d ", class2->individual_map[i],
	    class2->individual[i]);
	shown = 1;
    }
    if (class2->individual_255_used) {
	storeAppendPrintf(sentry, "%d:%d ", 255, class2->individual[255]);
	shown = 1;
    }
    if (!shown)
	storeAppendPrintf(sentry, "Not used yet.");
    storeAppendPrintf(sentry, "\n\n");
}

static void
delayPoolStats3(StoreEntry * sentry, unsigned short pool)
{
    /* fully malloc()d struct in this case only */
    delaySpecSet *rate = Config.Delay.rates[pool];
    class3DelayPool *class3 = delay_data[pool].class3;
    unsigned char shown = 0, i, j;

    storeAppendPrintf(sentry, "Pool: %d\n\tClass: 3\n\n", pool + 1);
    delayPoolStatsAg(sentry, rate, class3->aggregate);
    if (rate->network.restore_bps == -1) {
	storeAppendPrintf(sentry, "\tNetwork:\n\t\tDisabled.");
    } else {
	storeAppendPrintf(sentry, "\tNetwork:\n");
	storeAppendPrintf(sentry, "\t\tMax: %d\n", rate->network.max_bytes);
	storeAppendPrintf(sentry, "\t\tRate: %d\n", rate->network.restore_bps);
	storeAppendPrintf(sentry, "\t\tCurrent: ");
	for (i = 0;; i++) {
	    if (class3->network_map[i] == 255)
		break;
	    storeAppendPrintf(sentry, "%d:%d ", class3->network_map[i],
		class3->network[i]);
	    shown = 1;
	}
	if (class3->network_255_used) {
	    storeAppendPrintf(sentry, "%d:%d ", 255, class3->network[255]);
	    shown = 1;
	}
	if (!shown)
	    storeAppendPrintf(sentry, "Not used yet.");
    }
    storeAppendPrintf(sentry, "\n\n");
    shown = 0;
    if (rate->individual.restore_bps == -1) {
	storeAppendPrintf(sentry, "\tIndividual:\n\t\tDisabled.\n\n");
	return;
    }
    storeAppendPrintf(sentry, "\tIndividual:\n");
    storeAppendPrintf(sentry, "\t\tMax: %d\n", rate->individual.max_bytes);
    storeAppendPrintf(sentry, "\t\tRate: %d\n", rate->individual.restore_bps);
    for (i = 0;; i++) {
	if (class3->network_map[i] == 255)
	    break;
	storeAppendPrintf(sentry, "\t\tCurrent [Network %d]: ", class3->network_map[i]);
	shown = 1;
	for (j = 0;; j++) {
	    if (class3->individual_map[i][j] == 255)
		break;
	    storeAppendPrintf(sentry, "%d:%d ", class3->individual_map[i][j],
		class3->individual[(i << 8) | j]);
	}
	if (class3->individual_255_used[i / 8] & (1 << (i % 8))) {
	    storeAppendPrintf(sentry, "%d:%d ", 255, class3->individual[(i << 8) | 255]);
	}
	storeAppendPrintf(sentry, "\n");
    }
    if (class3->network_255_used) {
	storeAppendPrintf(sentry, "\t\tCurrent [Network 255]: ");
	shown = 1;
	for (j = 0;; j++) {
	    if (class3->individual_map[255][j] == 255)
		break;
	    storeAppendPrintf(sentry, "%d:%d ", class3->individual_map[255][j],
		class3->individual[(255 << 8) | j]);
	}
	if (class3->individual_255_used[255 / 8] & (1 << (255 % 8))) {
	    storeAppendPrintf(sentry, "%d:%d ", 255, class3->individual[(255 << 8) | 255]);
	}
	storeAppendPrintf(sentry, "\n");
    }
    if (!shown)
	storeAppendPrintf(sentry, "\t\tCurrent [All networks]: Not used yet.\n");
    storeAppendPrintf(sentry, "\n");
}

static void
delayPoolStats(StoreEntry * sentry)
{
    unsigned short i;

    storeAppendPrintf(sentry, "Delay pools configured: %d\n\n", Config.Delay.pools);
    for (i = 0; i < Config.Delay.pools; i++) {
	switch (Config.Delay.class[i]) {
	case 0:
	    storeAppendPrintf(sentry, "Pool: %d\n\tClass: 0\n\n", i + 1);
	    storeAppendPrintf(sentry, "\tMisconfigured pool.\n\n");
	    break;
	case 1:
	    delayPoolStats1(sentry, i);
	    break;
	case 2:
	    delayPoolStats2(sentry, i);
	    break;
	case 3:
	    delayPoolStats3(sentry, i);
	    break;
	default:
	    assert(0);
	}
    }
}

#endif

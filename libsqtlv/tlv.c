#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../include/util.h"

#include "tlv.h"

#if 0
MemPool * pool_swap_tlv = NULL;
#endif

void
tlv_init(void)
{
#if 0
    pool_swap_tlv = memPoolCreate("storeSwapTLV", sizeof(tlv));
#endif
}

tlv **
tlv_add(int type, const void *ptr, size_t len, tlv ** tail)
{
/*    tlv *t = memPoolAlloc(pool_swap_tlv); */
    tlv *t = xmalloc(sizeof(tlv));
    t->type = (char) type;
    t->length = (int) len;
    t->value = xmalloc(len);
    xmemcpy(t->value, ptr, len);
    *tail = t;
    return &t->next;            /* return new tail pointer */
}

void
tlv_free(tlv * n)
{
    tlv *t;
    while ((t = n) != NULL) {
        n = t->next;
        xfree(t->value);
/*        memPoolFree(pool_swap_tlv, t); */
        xfree(t);
    }
}

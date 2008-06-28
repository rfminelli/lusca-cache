
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/util.h"
#include "../include/Stack.h"
#include "../libcore/tools.h"
#include "../libcore/gb.h"
#include "../libmem/MemPool.h"

#include "intlist.h"

static MemPool * pool_intlist = NULL;

static void
intlistCheckAlloc(void)
{
	if (! pool_intlist)
		pool_intlist = memPoolCreate("intlist", sizeof(intlist));
}

void
intlistDestroy(intlist ** list)
{
    intlist *w = NULL;
    intlist *n = NULL;
    intlistCheckAlloc();
    for (w = *list; w; w = n) {
        n = w->next;
        memPoolFree(w, pool_intlist);
    }
    *list = NULL;
}

int
intlistFind(intlist * list, int i)
{
    intlist *w = NULL;
    for (w = list; w; w = w->next)
        if (w->i == i)
            return 1;
    return 0;
}

intlist *
intlistAddTail(intlist * list, int i)
{
    intlist *t, *n;

    for (t = list; t; t = t->next)
        ;

    intlistCheckAlloc();
    n = memPoolAlloc(pool_intlist);
    n->i = i;
    n->next = NULL;
    if (t)
      t->next = n;
    return n;
}

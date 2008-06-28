
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/util.h"
#include "../include/Stack.h"
#include "../libcore/tools.h"
#include "../libcore/gb.h"
#include "../libmem/MemPool.h"

#include "wordlist.h"

static MemPool * pool_wordlist = NULL;

static inline void
wordlistInitMem(void)
{
	if (! pool_wordlist)
		pool_wordlist = memPoolCreate("wordlist", sizeof(wordlist));
}

void
wordlistDestroy(wordlist ** list)
{   
    wordlist *w = NULL;
    while ((w = *list) != NULL) {
        *list = w->next;
        safe_free(w->key);
	wordlistInitMem();
        memPoolFree(pool_wordlist, w);
    }
    *list = NULL;
}

char *
wordlistAddBuf(wordlist ** list, const char *buf, int len)
{
    while (*list)
        list = &(*list)->next;
    wordlistInitMem();
    *list = memPoolAlloc(pool_wordlist);
    (*list)->key = xstrndup(buf, len);
    (*list)->next = NULL;
    return (*list)->key;
}

char *
wordlistAdd(wordlist ** list, const char *key)
{   
    while (*list)
        list = &(*list)->next;
    wordlistInitMem();
    *list = memPoolAlloc(pool_wordlist);
    (*list)->key = xstrdup(key);
    (*list)->next = NULL;
    return (*list)->key;
}


void
wordlistJoin(wordlist ** list, wordlist ** wl)
{   
    while (*list)
        list = &(*list)->next;
    *list = *wl;
    *wl = NULL;
}

void
wordlistAddWl(wordlist ** list, wordlist * wl)
{   
    while (*list)
        list = &(*list)->next;
    for (; wl; wl = wl->next, list = &(*list)->next) {
        wordlistInitMem();
        *list = memPoolAlloc(pool_wordlist);
        (*list)->key = xstrdup(wl->key);
        (*list)->next = NULL;
    }
}

#if 0
void
wordlistCat(const wordlist * w, MemBuf * mb)
{   
    while (NULL != w) {
        memBufPrintf(mb, "%s\n", w->key);
        w = w->next;
    }
}
#endif

wordlist *
wordlistDup(const wordlist * w)
{   
    wordlist *D = NULL;
    while (NULL != w) {
        wordlistAdd(&D, w->key);
        w = w->next;
    }
    return D;
}

char *
wordlistPopHead(wordlist **head)
{
	wordlist *e;
	char *k;

	if (*head == NULL)
		return NULL;
	
	k = (*head)->key;
	e = *head;
	*head = (*head)->next;
	wordlistInitMem();
	memPoolFree(pool_wordlist, e);
	return k;
}


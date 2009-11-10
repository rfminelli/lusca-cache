#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>


#include "include/Array.h"
#include "include/Stack.h"

#include "libcore/tools.h"
#include "libcore/radix.h"

static radix_node_t *
prefix_add(radix_tree_t *rt, const char *a)
{
	struct in_addr addr;
	prefix_t *p;
	radix_node_t *rn;

	(void) inet_aton(a, &addr);
	p = New_Prefix(AF_INET, &addr, 32, NULL);

	rn = radix_lookup(rt, p);
	printf("prefix_add: %s: -> %p\n", inet_ntoa(addr), rn);
	return rn;
}

static void
test1a(void)
{
	radix_tree_t *rt;
	radix_node_t *rn;
	Stack s;

	rt = New_Radix();

	/* add a prefix */
	rn = prefix_add(rt, "192.168.9.13");

	/* add a second prefix */
	rn = prefix_add(rt, "192.168.9.10");

	/* add a third prefix */
	rn = prefix_add(rt, "127.0.0.1");

	/* Now, walk the array, populating the stack */
	stackInit(&s);
	RADIX_WALK(rt->head, rn) {
		printf("adding: %p\n", rn);
		stackPush(&s, rn);
	} RADIX_WALK_END;

	/* now walk the stack, clearing things */
	while ( (rn = stackPop(&s)) != NULL) {
		printf("removing: %p\n", rn);
		radix_remove(rt, rn);
	}
}

int
main(int argc, const char *argv[])
{
	printf("%s: initializing\n", argv[0]);
	test1a();
	exit(0);
}


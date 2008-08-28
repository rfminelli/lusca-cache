#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "libmem/Vector.h"

static int
test1a(void)
{
	vector_t v;
	int *a;
	int i;

	vector_init(&v, sizeof(int), 16);

	a = vector_append(&v);
	printf("  appending 1 to %p\n", a);
	*a = 1;

	a = vector_append(&v);
	printf("  appending 2 to %p\n", a);
	*a = 2;

	a = vector_append(&v);
	printf("  appending 3 to %p\n", a);
	*a = 3;

	for (i = 0; i < vector_numentries(&v); i++) {
		a = vector_get(&v, i);
		printf("-> %p (%d)\n", a, *a);
	}

	vector_done(&v);
	return 1;
}

int
main(int argc, const char *argv[])
{
	printf("%s: initializing\n", argv[0]);
	test1a();
	exit(0);
}


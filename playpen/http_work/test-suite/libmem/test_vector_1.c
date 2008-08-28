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

static void
vector_print_int(vector_t *v)
{
	int i;
	int *a;

	for (i = 0; i < vector_numentries(v); i++) {
		a = vector_get(v, i);
		printf("-> %p (%d)\n", a, *a);
	}
}

static int
test1a(void)
{
	vector_t v;
	int *a;

	printf("test1a: basic setup, append, delete\n");
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

	vector_print_int(&v);

	vector_done(&v);
	return 1;
}

static int
test1b(void)
{
	vector_t v;
	int *a;

	printf("test1b: basic setup, append, insert in middle, delete\n");
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

	vector_print_int(&v);

	a = vector_insert(&v, 2);
	*a = 4;

	printf("after insert\n");
	vector_print_int(&v);

	vector_done(&v);
	return 1;
}

static int
test1c(void)
{
	vector_t v;
	int *a;

	printf("test1b: basic setup, append, insert at end, delete\n");
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

	vector_print_int(&v);

	printf(" inserting 4 at end\n");
	a = vector_insert(&v, 4);
	*a = 4;

	printf("after insert\n");
	vector_print_int(&v);

	vector_done(&v);
	return 1;
}



int
main(int argc, const char *argv[])
{
	printf("%s: initializing\n", argv[0]);
	test1a();
	test1b();
	test1c();
	exit(0);
}


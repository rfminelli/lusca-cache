#include "include/config.h"

#include <atf-c.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "include/Array.h"
#include "include/Stack.h"
#include "include/Vector.h"
#include "include/util.h"

#include "core.h"

ATF_TC(Vector_test_1);
ATF_TC_HEAD(Vector_test_1, tc)
{
	atf_tc_set_md_var(tc, "descr", "Vector create/delete test");
}
ATF_TC_BODY(Vector_test_1, tc)
{
	Vector v;

	vector_init(&v, 32, 128);
	vector_done(&v);
}

/* ** */

ATF_TC(Vector_test_2);
ATF_TC_HEAD(Vector_test_2, tc)
{
	atf_tc_set_md_var(tc, "descr", "Vector create/populate/delete test");
}
ATF_TC_BODY(Vector_test_2, tc)
{
	Vector v;
	int i, *n;

	vector_init(&v, sizeof(int), 128);
	for (i = 0; i < 16; i++) {
		n = (int *) vector_append(&v);
		ATF_REQUIRE(n != NULL);
		(*n) = i;
	}
	for (i = 0; i < 16; i++) {
		n = (int *) vector_get(&v, i);
		ATF_REQUIRE((*n) == i);
	}
	vector_done(&v);
}

/* ** */

ATF_TC(Vector_test_3);
ATF_TC_HEAD(Vector_test_3, tc)
{
	atf_tc_set_md_var(tc, "descr", "Vector bounds check");
}
ATF_TC_BODY(Vector_test_3, tc)
{
	Vector v;
	int i, *n;

	vector_init(&v, sizeof(int), 128);
	for (i = 0; i < 16; i++) {
		n = (int *) vector_append(&v);
		ATF_REQUIRE(n != NULL);
		(*n) = i;
		//printf("%d: %p; used %d\n", i, n, v.used_count);
	}
	//printf("check\n");
	for (i = 0; i < 32; i++) {
		n = (int *) vector_get_real(&v, i);
		if (i < 16) {
			//printf("%d: %p; used %d\n", i, n, v.used_count);
			ATF_REQUIRE((*n) == i);
		} else {
			//printf("%d: %p; used %d\n", i, n, v.used_count);
			ATF_REQUIRE(n == NULL);
		}
	}
	vector_done(&v);
}

/* ** */
ATF_TC(Vector_insert_1);
ATF_TC_HEAD(Vector_insert_1, tc)
{
	atf_tc_set_md_var(tc, "descr", "Vector insert checks");
}
ATF_TC_BODY(Vector_insert_1, tc)
{
	Vector v;
	int i, *n;

	vector_init(&v, sizeof(int), 128);
	for (i = 0; i < 16; i++) {
		n = (int *) vector_append(&v);
		ATF_REQUIRE(n != NULL);
		(*n) = i;
		//printf("%d: %p; used %d\n", i, n, v.used_count);
	}

	for (i = 0; i < 32; i += 2) {
		n = (int *) vector_insert(&v, i);
	}
	for (i = 0; i < v.used_count; i++) {
		n = (int *) vector_get_real(&v, i);
		ATF_REQUIRE(n != NULL);
		//printf("%d: %d (%d)\n", i, (*n), i / 2);
		ATF_REQUIRE_EQ( (*n), i / 2);
	}
}


ATF_TP_ADD_TCS(tp)
{
        ATF_TP_ADD_TC(tp, Vector_test_1);
        ATF_TP_ADD_TC(tp, Vector_test_2);
        ATF_TP_ADD_TC(tp, Vector_test_3);
        ATF_TP_ADD_TC(tp, Vector_insert_1);
}

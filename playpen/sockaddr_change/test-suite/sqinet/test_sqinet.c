#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <netdb.h>

#include "libsqinet/sqinet.h"

static int
test1a(void)
{
	sqaddr_t s;
	int e;
	char buf[128];
	struct in_addr a;

	printf("  sqinet_aton(""127.0.0.1"") parses correctly: ");
	sqinet_init(&s);
	e = sqinet_aton(&s, "127.0.0.1", SQATON_FAMILY_IPv4);
	if (! e) {
		printf("FAILED: retval != 0\n");
		return 0;
	}
	a = sqinet_get_v4_inaddr(&s, SQADDR_ASSERT_IS_V4);
	if (a.s_addr != htonl(0x7f000001)) {
		printf("FAILED: a.s_addr != htonl(0x7f000001) (%X)\n", ntohl(a.s_addr));
		return 0;
	}
	printf("OK - htonl(0x7f000001)\n");
	return 1;
}

static int
test1b(void)
{
	sqaddr_t s;
	int e;
	char buf[128];
	struct in_addr a;

	printf("  sqinet_aton(""0.0.0.0"") parses correctly as INADDR_ANY: ");
	sqinet_init(&s);
	e = sqinet_aton(&s, "0.0.0.0", SQATON_FAMILY_IPv4);
	if (! e) {
		printf("FAILED: retval != 0\n");
		return 0;
	}
	a = sqinet_get_v4_inaddr(&s, SQADDR_ASSERT_IS_V4);
	if (a.s_addr != htonl(0x00000000)) {
		printf("FAILED: a.s_addr != htonl(0x00000000) (%X)\n", ntohl(a.s_addr));
		return 0;
	}

	if (! sqinet_is_anyaddr(&s)) {
		printf("FAILED: sqinet_is_anyaddr(&s) is false!\n");
		return 0;
	}
	printf("OK - 0.0.0.0 is anyaddr\n");
}

int
main(int argc, const char *argv[])
{
	printf("Test 1: IPv4 localhost address checks:\n");
	test1a();	
	test1b();	
}

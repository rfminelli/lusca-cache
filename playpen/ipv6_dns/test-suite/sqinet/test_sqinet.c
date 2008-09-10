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

#include "libsqinet/inet_legacy.h"
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
	buf[0] = 0;
	e = sqinet_assemble_rev(&s, buf, 128);
	printf("  revdns is %s\n", buf);
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
	if (sqinet_is_noaddr(&s)) {
		printf("FAILED: sqinet_is_noaddr(&s) is true when it should be false!\n");
		return 0;
	}
	if (! IsAnyAddr(&a)) {
		printf("FAILED: IsAnyAddr(&a) is false!\n");
		return 0;
	}
	if (IsNoAddr(&a)) {
		printf("FAILED: IsNoAddr(&a) is true when it should be false!\n");
		return 0;
	}
	buf[0] = 0;
	printf("OK - 0.0.0.0 is anyaddr\n");
	e = sqinet_assemble_rev(&s, buf, 128);
	printf("  revdns is %s\n", buf);
}

static int
test1c(void)
{
	sqaddr_t s;
	int e;
	char buf[128];
	struct in_addr a;

	printf("  sqinet_aton(""255.255.255.255"") parses correctly as INADDR_NONE: ");
	sqinet_init(&s);
	e = sqinet_aton(&s, "255.255.255.255", SQATON_FAMILY_IPv4);
	if (! e) {
		printf("FAILED: retval != 0\n");
		return 0;
	}
	a = sqinet_get_v4_inaddr(&s, SQADDR_ASSERT_IS_V4);
	if (a.s_addr != htonl(0xffffffff)) {
		printf("FAILED: a.s_addr != htonl(0xffffffff) (%X)\n", ntohl(a.s_addr));
		return 0;
	}

	if (! sqinet_is_noaddr(&s)) {
		printf("FAILED: sqinet_is_noaddr(&s) is false!\n");
		return 0;
	}
	if (sqinet_is_anyaddr(&s)) {
		printf("FAILED: sqinet_is_anyaddr(&s) is true when it should be false!\n");
		return 0;
	}
	if (IsAnyAddr(&a)) {
		printf("FAILED: IsAnyAddr(&a) is true when it should be false!\n");
		return 0;
	}
	if (! IsNoAddr(&a)) {
		printf("FAILED: IsNoAddr(&a) is false!\n");
		return 0;
	}
	printf("OK - 255.255.255.255 is noaddr\n");
	buf[0] = 0;
	e = sqinet_assemble_rev(&s, buf, 128);
	printf("  revdns is %s\n", buf);
}

static int
test2a(void)
{
	sqaddr_t s;
	int e;
	char buf[128];
	struct in_addr a;

	printf("  sqinet_aton(""::0"") parses correctly as INADDR_ANY: ");
	sqinet_init(&s);
	e = sqinet_aton(&s, "::0", SQATON_FAMILY_IPv6);
	if (! e) {
		printf("FAILED: retval != 0\n");
		return 0;
	}
	if (sqinet_is_noaddr(&s)) {
		printf("FAILED: sqinet_is_noaddr(&s) is true when it should be false!\n");
		return 0;
	}
	if (! sqinet_is_anyaddr(&s)) {
		printf("FAILED: sqinet_is_anyaddr(&s) is false when it should be true!\n");
		return 0;
	}
	buf[0] = 0;
	e = sqinet_assemble_rev(&s, buf, 128);
	printf("  revdns is %s\n", buf);
	printf("OK - ::0 is noaddr\n");
}


int
main(int argc, const char *argv[])
{
	printf("Test 1: IPv4 address checks:\n");
	test1a();
	test1b();
	test1c();
	printf("Test 2: IPv6 address checks:\n");
	test2a();
}

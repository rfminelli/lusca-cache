
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
#include "include/util.h"
#include "libcore/valgrind.h"
#include "libcore/varargs.h"
#include "libcore/debug.h"
#include "libcore/kb.h"
#include "libcore/gb.h"
#include "libcore/tools.h"

#include "libmem/MemPool.h"
#include "libmem/MemBufs.h"
#include "libmem/MemBuf.h"
#include "libmem/String.h"
#include "libmem/MemStr.h"

#include "libcb/cbdata.h"

#include "libstat/StatHist.h"

#include "libsqinet/inet_legacy.h"
#include "libsqinet/sqinet.h"

#include "libhttp/HttpVersion.h"
#include "libhttp/HttpStatusLine.h"
#include "libhttp/HttpHeaderType.h"
#include "libhttp/HttpHeaderFieldStat.h"
#include "libhttp/HttpHeaderFieldInfo.h"
#include "libhttp/HttpHeaderEntry.h"
#include "libhttp/HttpHeader.h"
#include "libhttp/HttpHeaderStats.h"
#include "libhttp/HttpHeaderTools.h"
#include "libhttp/HttpHeaderMask.h"
#include "libhttp/HttpHeaderParse.h"

#include "core.h"

extern int hh_check_content_length(HttpHeader *hdr, const char *val, int vlen);

static void
libhttp_test_parser(const char *str, int ret)
{
	HttpHeader hdr;

	test_core_init();
	httpHeaderInitLibrary();
	ATF_CHECK_EQ(test_core_parse_header(&hdr, str), ret);
	httpHeaderClean(&hdr);
}

extern int hh_check_content_length(HttpHeader *hdr, const char *val, int vlen);

static int
test_http_content_length(HttpHeader *hdr, const char *str)
{
	int r;

	/* XXX remember; this may delete items from the header entry array! */
	r = hh_check_content_length(hdr, str, strlen(str));
	return r;
}

/* *** */

ATF_TC(libhttp_parse_1);
ATF_TC_HEAD(libhttp_parse_1, tc)
{
	atf_tc_set_md_var(tc, "descr", "libhttp_parse_1");
}

ATF_TC_BODY(libhttp_parse_1, tc)
{
	libhttp_test_parser("Host: www.creative.net.au\r\nContent-type: text/html\r\nFoo: bar\r\n\r\n", 1);
}

ATF_TC(libhttp_parse_2);
ATF_TC_HEAD(libhttp_parse_2, tc)
{
	atf_tc_set_md_var(tc, "descr", "content-length header parsing");
}

ATF_TC_BODY(libhttp_parse_2, tc)
{
	libhttp_test_parser("Host: www.creative.net.au\r\nContent-Length: 12345\r\nContent-type: text/html\r\nFoo: bar\r\n\r\n", 1);
}

/* ** */

ATF_TC(libhttp_parse_3);
ATF_TC_HEAD(libhttp_parse_3, tc)
{
	atf_tc_set_md_var(tc, "descr", "content-length header parsing - failure");
}

ATF_TC_BODY(libhttp_parse_3, tc)
{
	libhttp_test_parser("Host: www.creative.net.au\r\nContent-Length: b12345\r\nContent-type: text/html\r\nFoo: bar\r\n\r\n", 0);
}

/* *** */

ATF_TC(libhttp_parse_4);
ATF_TC_HEAD(libhttp_parse_4, tc)
{
	atf_tc_set_md_var(tc, "descr", "content-length header parsing - two conflicting Content-Length headers; failure");
}

ATF_TC_BODY(libhttp_parse_4, tc)
{
	HttpHeader hdr;
	libhttp_test_parser("Host: www.creative.net.au\r\nContent-Length: 12345\r\nContent-type: text/html\r\nFoo: bar\r\nContent-Length: 23456\r\n", 0);
}

/* *** */


ATF_TC(libhttp_parse_content_length_1);

ATF_TC_HEAD(libhttp_parse_content_length_1, tc)
{
	atf_tc_set_md_var(tc, "descr", "libhttp_parse_content_length_1");
}

ATF_TC_BODY(libhttp_parse_content_length_1, tc)
{
	HttpHeader hdr;
	int ret;
	const char *hdrs = "Host: www.creative.net.au\r\nContent-Length: 12345\r\nContent-type: text/html\r\nFoo: bar\r\n\r\n";
	const char *hdr_start = hdrs;
	const char *hdr_end = hdr_start + strlen(hdrs);

	test_core_init();

        httpHeaderInitLibrary();
	httpHeaderInit(&hdr, hoRequest);

	ATF_REQUIRE(test_http_content_length(&hdr, "12345") == 1);
	ATF_REQUIRE(test_http_content_length(&hdr, "123b5") == 1);
	ATF_REQUIRE(test_http_content_length(&hdr, "b1234") == -1);
	ATF_REQUIRE(test_http_content_length(&hdr, "abcde") == -1);

	/* now check duplicates */
	ATF_REQUIRE(httpHeaderParse(&hdr, hdr_start, hdr_end) == 1);

#if 0
	printf("test1c: hh_check_content_length: 12345 = %d\n", test_hh_content_length(&hdr, "12345"));
	printf("test1c: hh_check_content_length: 123b5 = %d\n", test_hh_content_length(&hdr, "123b5"));
	printf("test1c: hh_check_content_length: b1234 = %d\n", test_hh_content_length(&hdr, "b1234"));
	printf("test1c: hh_check_content_length: 12344 = %d\n", test_hh_content_length(&hdr, "12344"));
	printf("test1c: hh_check_content_length: 12346 = %d\n", test_hh_content_length(&hdr, "12346"));
	printf("test1c: hh_check_content_length: 12344 = %d\n", test_hh_content_length(&hdr, "12344"));

	printf("test1c: hh_check_content_length: setting httpConfig_relaxed_parser to 1 (ok)\n");
	httpConfig_relaxed_parser = 1;
	printf("test1c: hh_check_content_length: 12345 = %d\n", test_hh_content_length(&hdr, "12345"));
	printf("test1c: hh_check_content_length: 123b5 = %d\n", test_hh_content_length(&hdr, "123b5"));
	printf("test1c: hh_check_content_length: b1234 = %d\n", test_hh_content_length(&hdr, "b1234"));

	printf("test1c: hh_check_content_length: 12344 = %d\n", test_hh_content_length(&hdr, "12344"));
	/* this one should result in the deletion of the "12345" entry from the original request parse */
	printf("test1c: hh_check_content_length: 12346 = %d\n", test_hh_content_length(&hdr, "12346"));
	printf("test1c: hh_check_content_length: 12344 = %d\n", test_hh_content_length(&hdr, "12344"));
#endif

	/* Clean up */
	httpHeaderReset(&hdr);
	httpHeaderClean(&hdr);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, libhttp_parse_1);
	ATF_TP_ADD_TC(tp, libhttp_parse_2);
	ATF_TP_ADD_TC(tp, libhttp_parse_3);
	ATF_TP_ADD_TC(tp, libhttp_parse_4);
	ATF_TP_ADD_TC(tp, libhttp_parse_content_length_1);
	return atf_no_error();
}


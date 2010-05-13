
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

static void
libhttp_test_content_length_parser(const char *str, const char *clength)
{
	HttpHeader hdr;
	HttpHeaderEntry *e;

	ATF_CHECK_EQ(test_core_parse_header(&hdr, str), 1);

	/* Verify the content-length header is what it should be */
	e = httpHeaderFindEntry(&hdr, HDR_CONTENT_LENGTH);
	ATF_REQUIRE(e != NULL);
	ATF_REQUIRE(strNCmp(e->value, clength, strlen(clength)) == 0);

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

	test_core_init();
        httpHeaderInitLibrary();
	httpHeaderInit(&hdr, hoRequest);

	ATF_REQUIRE(test_http_content_length(&hdr, "12345") == 1);
	ATF_REQUIRE(test_http_content_length(&hdr, "123b5") == 1);
	ATF_REQUIRE(test_http_content_length(&hdr, "b1234") == -1);
	ATF_REQUIRE(test_http_content_length(&hdr, "abcde") == -1);

	/* Clean up */
	httpHeaderReset(&hdr);
	httpHeaderClean(&hdr);
}

ATF_TC(libhttp_parse_content_length_2);
ATF_TC_HEAD(libhttp_parse_content_length_2, tc)
{
	atf_tc_set_md_var(tc, "descr", "Check that duplicate Content-Length headers are "
	    "correctly replaced with the relaxed HTTP parser enabled");
}
ATF_TC_BODY(libhttp_parse_content_length_2, tc)
{
	test_core_init();
	httpHeaderInitLibrary();
	httpConfig_relaxed_parser = 1;
	libhttp_test_content_length_parser("Content-Length: 12345\r\nContent-Length: 23456\r\n", "23456");
	libhttp_test_content_length_parser("Content-Length: 23456\r\nContent-Length: 12345\r\n", "23456");
	libhttp_test_content_length_parser("Content-Length: 23456\r\nContent-Length: 12345\r\nContent-Length: 23456\r\n", "23456");
}

ATF_TC(libhttp_parser_other_whitespace_1);
ATF_TC_HEAD(libhttp_parser_other_whitespace_1, tc)
{
	atf_tc_set_md_var(tc, "descr", "Headers must not have whitespace in the field names");
}
ATF_TC_BODY(libhttp_parser_other_whitespace_1, tc)
{
	libhttp_test_parser("Fo o: bar\r\n", 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, libhttp_parse_1);
	ATF_TP_ADD_TC(tp, libhttp_parse_2);
	ATF_TP_ADD_TC(tp, libhttp_parse_3);
	ATF_TP_ADD_TC(tp, libhttp_parse_4);
	ATF_TP_ADD_TC(tp, libhttp_parse_content_length_1);
	ATF_TP_ADD_TC(tp, libhttp_parse_content_length_2);
	ATF_TP_ADD_TC(tp, libhttp_parser_other_whitespace_1);
	return atf_no_error();
}



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

ATF_TC_WITH_CLEANUP(libhttp_parse_1);

ATF_TC_HEAD(libhttp_parse_1, tc)
{
	atf_tc_set_md_var(tc, "descr", "libhttp_parse_1");
}

ATF_TC_BODY(libhttp_parse_1, tc)
{
	HttpHeader hdr;
#if 0
	HttpHeaderPos pos = HttpHeaderInitPos;
	const HttpHeaderEntry *e;
#endif

	const char *hdrs = "Host: www.creative.net.au\r\nContent-type: text/html\r\nFoo: bar\r\n\r\n";
	const char *hdr_start = hdrs;
	const char *hdr_end = hdr_start + strlen(hdrs);

	test_core_init();

	httpHeaderInitLibrary();
	httpHeaderInit(&hdr, hoRequest);

	ATF_REQUIRE(httpHeaderParse(&hdr, hdr_start, hdr_end) == 1);

	httpHeaderClean(&hdr);
}

ATF_TC_CLEANUP(libhttp_parse_1, tc)
{
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, libhttp_parse_1);
	return atf_no_error();
}


#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <strings.h>

#include "sqinet.h"

const char *
xinet_ntoa(const struct in_addr addr)
{
    return inet_ntoa(addr);
}

void
sqinet_init(sqaddr_t *s)
{
}

void
sqinet_done(sqaddr_t *s)
{
}

int
sqinet_copy_v4_inaddr(sqaddr_t *src, struct in_addr *dst, sqaddr_flags flags)
{
	struct sockaddr_in *s;

	/* Must be a v4 address */
	if (flags & SQADDR_ASSERT_IS_V4)
		assert(src->st.ss_family == AF_INET);
	if (src->st.ss_family != AF_INET)
		return 0;

	s = (struct sockaddr_in *) &src->st;
	*dst = s->sin_addr;
	return 1;
}

int
sqinet_set_v4_inaddr(sqaddr_t *s, struct in_addr *v4addr)
{
	struct sockaddr_in *v4;

	s->st.ss_family = AF_INET;

	v4 = (struct sockaddr_in *) &s->st;
	v4->sin_family = AF_INET;		/* XXX is this needed? Its a union after all.. */
	v4->sin_addr = *v4addr;
	v4->sin_port = 0;
	return 1;
}

int
sqinet_set_v4_sockaddr(sqaddr_t *s, struct sockaddr_in *v4addr)
{
	struct sockaddr_in *v4;

	v4 = (struct sockaddr_in *) &s->st;
	*v4 = *v4addr;
	s->st.ss_family = AF_INET;
	return 1;
}

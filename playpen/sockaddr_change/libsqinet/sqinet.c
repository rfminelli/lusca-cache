#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <strings.h>
#include <netdb.h>

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
sqinet_copy_v4_inaddr(const sqaddr_t *src, struct in_addr *dst, sqaddr_flags flags)
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
sqinet_set_v4_port(sqaddr_t *s, short port, sqaddr_flags flags)
{
	struct sockaddr_in *v4;

	/* Must be a v4 address */
	if (flags & SQADDR_ASSERT_IS_V4)
		assert(s->st.ss_family == AF_INET);
	if (s->st.ss_family != AF_INET)
		return 0;
	v4 = (struct sockaddr_in *) &s->st;
	v4->sin_port = htons(port);
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

struct in_addr
sqinet_get_v4_inaddr(const sqaddr_t *s, sqaddr_flags flags)
{
	struct sockaddr_in *v4;

	if (flags & SQADDR_ASSERT_IS_V4) {
		assert(s->st.ss_family == AF_INET);
	}
	v4 = (struct sockaddr_in *) &s->st;
	return v4->sin_addr;
}

int
sqinet_is_anyaddr(const sqaddr_t *s)
{
	struct sockaddr_in *v4;

	/* XXX for now, only handle v4 */
	assert(s->st.ss_family == AF_INET);

	v4 = (struct sockaddr_in *) &s->st;
	return (v4->sin_addr.s_addr == INADDR_ANY);
}

int
sqinet_is_noaddr(const sqaddr_t *s)
{
	struct sockaddr_in *v4;

	/* XXX for now, only handle v4 */
	assert(s->st.ss_family == AF_INET);

	v4 = (struct sockaddr_in *) &s->st;
	return (v4->sin_addr.s_addr == INADDR_NONE);
}

int
sqinet_get_port(const sqaddr_t *s)
{
	switch (s->st.ss_family) {
		case AF_INET:
			return ntohs(((struct sockaddr_in *) &s->st)->sin_port);
		case AF_INET6:
			return ntohs(((struct sockaddr_in6 *) &s->st)->sin6_port);
		default:
			assert(0);
	}
	return 0;
}

int
sqinet_ntoa(const sqaddr_t *s, char *hoststr, int hostlen, sqaddr_flags flags)
{
	if (flags & SQADDR_ASSERT_IS_V4)
		assert(s->st.ss_family == AF_INET);
	if (flags & SQADDR_ASSERT_IS_V6)
		assert(s->st.ss_family == AF_INET6);

	return getnameinfo((struct sockaddr *) (&s->st), sqinet_get_length(s), hoststr, hostlen, NULL, 0, NI_NUMERICHOST|NI_NUMERICSERV);
}

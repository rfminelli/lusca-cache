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

#include "sqinet.h"

const char *
xinet_ntoa(const struct in_addr addr)
{
    return inet_ntoa(addr);
}

int
IsNoAddr(const struct in_addr *s)
{
	return s->s_addr == INADDR_NONE;
}

int
IsAnyAddr(const struct in_addr *s)
{
	return s->s_addr == INADDR_ANY;
}

void
SetNoAddr(struct in_addr *s)
{
	s->s_addr = INADDR_NONE;
}

void
SetAnyAddr(struct in_addr *s)
{
	s->s_addr = INADDR_ANY;
}

void
sqinet_init(sqaddr_t *s)
{
	bzero(s, sizeof(*s));
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
sqinet_get_v4_sockaddr_ptr(const sqaddr_t *s, struct sockaddr_in *v4, sqaddr_flags flags)
{
	if (flags & SQADDR_ASSERT_IS_V4)
		assert(s->st.ss_family == AF_INET);
	if (flags & SQADDR_ASSERT_IS_V6)
		assert(s->st.ss_family == AF_INET6);

	*v4 = *(struct sockaddr_in *) &s->st;
	return 1;
}

struct sockaddr_in
sqinet_get_v4_sockaddr(const sqaddr_t *s, sqaddr_flags flags)
{
	if (flags & SQADDR_ASSERT_IS_V4)
		assert(s->st.ss_family == AF_INET);
	if (flags & SQADDR_ASSERT_IS_V6)
		assert(s->st.ss_family == AF_INET6);

	return * (struct sockaddr_in *) &s->st;
}

int
sqinet_set_v6_sockaddr(sqaddr_t *s, struct sockaddr_in6 *v6addr)
{
	struct sockaddr_in6 *v6;

	v6 = (struct sockaddr_in6 *) &s->st;
	*v6 = *v6addr;
	s->st.ss_family = AF_INET6;
	return 1;
}


int
sqinet_is_anyaddr(const sqaddr_t *s)
{
	struct sockaddr_in *v4;
	struct sockaddr_in6 *v6;
	struct in6_addr any6addr = IN6ADDR_ANY_INIT;

	switch(s->st.ss_family) {
		case AF_INET:
			v4 = (struct sockaddr_in *) &s->st;
			return (v4->sin_addr.s_addr == INADDR_ANY);
			break;
		case AF_INET6:
			v6 = (struct sockaddr_in6 *) &s->st;
			return (memcmp(&v6->sin6_addr, &any6addr, sizeof(any6addr)) == 0);
			break;
		default:
			assert(0);
	}
	return 0;
}

int
sqinet_is_noaddr(const sqaddr_t *s)
{
	struct sockaddr_in *v4;
	struct sockaddr_in6 *v6;
	struct in6_addr no6addr = {{{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }}};

	switch(s->st.ss_family) {
		case AF_INET:
			v4 = (struct sockaddr_in *) &s->st;
			return (v4->sin_addr.s_addr == INADDR_NONE);
			break;
		case AF_INET6:
			v6 = (struct sockaddr_in6 *) &s->st;
			return (memcmp(&v6->sin6_addr, &no6addr, sizeof(no6addr)) == 0);
			break;
		default:
			assert(0);
	}
	return 0;
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

void
sqinet_set_port(const sqaddr_t *s, short port, sqaddr_flags flags)
{
	if (flags & SQADDR_ASSERT_IS_V4)
		assert(s->st.ss_family == AF_INET);
	if (flags & SQADDR_ASSERT_IS_V6)
		assert(s->st.ss_family == AF_INET6);

	switch (s->st.ss_family) {
		case AF_INET:
			((struct sockaddr_in *) &s->st)->sin_port = htons(port);
		case AF_INET6:
			((struct sockaddr_in6 *) &s->st)->sin6_port = htons(port);
		default:
			assert(0);
	}
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

/*
 * Perform a IP string -> sockaddr translation.
 *
 * The sqaddr_t must be init'ed (not asserted at the moment.)
 * The port won't be filled in - the caller can do that with a sqinet_set_port() call
 * when its written.
 */
int
sqinet_aton(sqaddr_t *s, char *hoststr, sqaton_flags flags)
{
	struct addrinfo hints, *r = NULL;
	int err;

	bzero(&hints, sizeof(hints));
	if (flags & SQATON_FAMILY_IPv4)
		hints.ai_family = AF_INET;
	if (flags & SQATON_FAMILY_IPv6)
		hints.ai_family = AF_INET6;
	if (flags & SQATON_PASSIVE)
		hints.ai_flags |= AI_PASSIVE;

	err = getaddrinfo(hoststr, NULL, &hints, &r);
	if (err != 0) {
		if (r != NULL)
			freeaddrinfo(r);
		return 0;
	}
	if (r == NULL) {
		return 0;
	}

	/* Just set the current sqaddr_t st to the first res pointer */
	/* We need to ensure that the lengths are compatible */
	/*
	 * Its a bit annoying that this API copies the data when most instances
	 * the caller is using this to do some kind of parsing and can use r->ai_addr
	 * direct. We may wish to replace this with a seperate function to -just- do
	 * IP string (+ port) -> sockaddr conversion which bypasses the damned
	 * allocation + copy overhead. Only if it matters..
	 */
	assert(r->ai_addrlen <= sizeof(s->st));
	memcpy(&s->st, r->ai_addr, r->ai_addrlen);
	freeaddrinfo(r);
	return 1;
}

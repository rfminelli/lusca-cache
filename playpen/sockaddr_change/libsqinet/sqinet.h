#ifndef	__LIBSQINET_INET_H__
#define	__LIBSQINET_INET_H__

struct _sqaddr {
	struct sockaddr_storage st;
};
typedef struct _sqaddr sqaddr_t;

typedef enum {
	SQADDR_NONE = 0x00,
	SQADDR_ASSERT_IS_V4 = 0x01,
	SQADDR_ASSERT_IS_V6 = 0x02,
} sqaddr_flags;




void sqinet_init(sqaddr_t *s);
void sqinet_done(sqaddr_t *s);
int sqinet_copy_v4_inaddr(sqaddr_t *s, struct in_addr *dst, sqaddr_flags flags);
int sqinet_set_v4_inaddr(sqaddr_t *s, struct in_addr *v4addr);
int sqinet_set_v4_sockaddr(sqaddr_t *s, struct sockaddr_in *v4addr);
int sqinet_set_v6_sockaddr(sqaddr_t *s, struct sockaddr_in6 *v6addr);

extern const char *xinet_ntoa(const struct in_addr addr);


#endif

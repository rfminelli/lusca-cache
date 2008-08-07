#ifndef	__LIBSQINET_INET_H__
#define	__LIBSQINET_INET_H__

#define	MAX_IPSTRLEN	75

struct _sqaddr {
	struct sockaddr_storage st;
};
typedef struct _sqaddr sqaddr_t;

typedef enum {
	SQADDR_NONE = 0x00,
	SQADDR_ASSERT_IS_V4 = 0x01,
	SQADDR_ASSERT_IS_V6 = 0x02,
} sqaddr_flags;


extern void sqinet_init(sqaddr_t *s);
extern void sqinet_done(sqaddr_t *s);
extern int sqinet_copy_v4_inaddr(const sqaddr_t *s, struct in_addr *dst, sqaddr_flags flags);
extern int sqinet_set_v4_inaddr(sqaddr_t *s, struct in_addr *v4addr);
extern int sqinet_set_v4_sockaddr(sqaddr_t *s, struct sockaddr_in *v4addr);
extern int sqinet_set_v6_sockaddr(sqaddr_t *s, struct sockaddr_in6 *v6addr);
extern struct in_addr sqinet_get_v4_inaddr(const sqaddr_t *s, sqaddr_flags flags);
extern int sqinet_is_anyaddr(const sqaddr_t *s);

extern const char *xinet_ntoa(const struct in_addr addr);

static inline struct sockaddr * sqinet_get_entry(sqaddr_t *s) { return (struct sockaddr *) &(s->st); }
/* XXX for now */


#endif

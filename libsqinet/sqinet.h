#ifndef	__LIBSQINET_INET_H__
#define	__LIBSQINET_INET_H__

#define	MAX_IPSTRLEN	75

struct _sqaddr {
	int init;
	struct sockaddr_storage st;
};
typedef struct _sqaddr sqaddr_t;

typedef enum {
	SQADDR_NONE = 0x00,
	SQADDR_ASSERT_IS_V4 = 0x01,
	SQADDR_ASSERT_IS_V6 = 0x02,
} sqaddr_flags;

typedef enum {
	SQATON_NONE = 0x0,
	SQATON_FAMILY_IPv4 = 0x2,
	SQATON_FAMILY_IPv6 = 0x4,
	SQATON_PASSIVE = 0x8
} sqaton_flags;

extern void sqinet_init(sqaddr_t *s);
extern void sqinet_done(sqaddr_t *s);
extern int sqinet_copy_v4_inaddr(const sqaddr_t *s, struct in_addr *dst, sqaddr_flags flags);
extern int sqinet_set_v4_inaddr(sqaddr_t *s, struct in_addr *v4addr);
extern int sqinet_set_v4_port(sqaddr_t *s, short port, sqaddr_flags flags);
extern int sqinet_set_v4_sockaddr(sqaddr_t *s, const struct sockaddr_in *v4addr);
extern int sqinet_set_v6_sockaddr(sqaddr_t *s, const struct sockaddr_in6 *v6addr);
extern int sqinet_get_port(const sqaddr_t *s);
extern void sqinet_set_port(const sqaddr_t *s, short port, sqaddr_flags flags);
extern struct in_addr sqinet_get_v4_inaddr(const sqaddr_t *s, sqaddr_flags flags);
extern int sqinet_get_v4_sockaddr_ptr(const sqaddr_t *s, struct sockaddr_in *v4, sqaddr_flags flags);
extern struct sockaddr_in sqinet_get_v4_sockaddr(const sqaddr_t *s, sqaddr_flags flags);
extern int sqinet_is_anyaddr(const sqaddr_t *s);
extern int sqinet_is_noaddr(const sqaddr_t *s);
extern int sqinet_ntoa(const sqaddr_t *s, char *hoststr, int hostlen, sqaddr_flags flags);
extern int sqinet_aton(sqaddr_t *s, char *hoststr, sqaton_flags flags);

static inline struct sockaddr * sqinet_get_entry(sqaddr_t *s) { return (struct sockaddr *) &(s->st); }
static inline int sqinet_get_family(const sqaddr_t *s) { return s->st.ss_family; }
static inline int sqinet_get_length(const sqaddr_t *s) { if (s->st.ss_family == AF_INET) return sizeof(struct sockaddr_in); else return sizeof(struct sockaddr_in6); }
static inline int sqinet_get_maxlength(const sqaddr_t *s) { return sizeof(s->st); }

static inline int sqinet_copy(sqaddr_t *dst, const sqaddr_t *src) { *dst = *src; return 1; }

#endif

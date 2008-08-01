#ifndef	__LIBSQINET_INET_H__
#define	__LIBSQINET_INET_H__

struct _sqaddr {
	struct sockaddr_storage st;
};

typedef struct _sqaddr sqaddr_t;


int sqinet_copy_v4_inaddr(sqaddr_t *src, struct struct in_addr *dst, int flags);

extern const char *xinet_ntoa(const struct in_addr addr);


#endif

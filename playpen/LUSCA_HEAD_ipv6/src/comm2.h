#ifndef	__SQUID_COMM2_H__
#define	__SQUID_COMM2_H__

typedef struct {
    char *host;
    u_short port;
    CNCB *callback;
    void *data;
    sqaddr_t in_addr6;
    int fd;
    int tries;
    int addrcount;
    int connstart;
    const char *comm_note;
    int comm_tos;
    int comm_flags;
} ConnectStateDataNew;

extern void
commConnectStartNew(const char *host, u_short port, CNCB * callback,
    void *data, sqaddr_t *addr6, int flags, int tos, const char *note);

#endif	/* __SQUID_COMM2_H__ */

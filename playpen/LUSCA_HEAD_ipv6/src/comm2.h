#ifndef	__SQUID_COMM2_H__
#define	__SQUID_COMM2_H__

typedef struct {
    char *host;
    u_short port;
    CNCB *callback;
    void *data;
    sqaddr_t in_addr6;
    sqaddr_t lcl_addr4;	/* outgoing_addr for v4 sockets */
    sqaddr_t lcl_addr6;	/* outgoing_addr for v6 sockets */
    int fd;
    int tries;
    int addrcount;
    int connstart;
    const char *comm_note;
    int comm_tos;
    int comm_flags;

    /*
     * Since the forward code had a timeout handler on the
     * FD which was called regardless of what the current
     * connection state was (or how many attempst, etc),
     * we need to replicate that here.
     */
    time_t start_time;		/* When was the connection started? */
    int timeout;		/* How long is the timeout? */
} ConnectStateDataNew;

extern ConnectStateDataNew *commConnectStartNewSetup(const char *host,
  u_short port, CNCB * callback, void *data, sqaddr_t *addr6, int flags,
  int tos, const char *note);

extern void commConnectStartNewBegin(ConnectStateDataNew *cs);
extern void commConnectNewSetupOutgoingV4(ConnectStateDataNew *cs,
  struct in_addr lcl);
extern void commConnectNewSetupOutgoingV6(ConnectStateDataNew *cs,
  sqaddr_t *lcl);
extern void commConnectNewSetTimeout(ConnectStateDataNew *cs,
  int timeout);

#endif	/* __SQUID_COMM2_H__ */

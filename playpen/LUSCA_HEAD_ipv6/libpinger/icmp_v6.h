#ifndef	__LIBPINGER_ICMP_V6_H__
#define	__LIBPINGER_ICMP_V6_H__

/* XXX this must match the v4 define! */
#define MAX_PAYLOAD     SQUIDHOSTNAMELEN

extern const char *icmp6LowPktStr[];
extern const char *icmp6HighPktStr[];

struct pingerv6_state {
	int icmp_ident;
	int icmp_pkts_sent;
	int icmp_sock;
};

extern void pingerv6SendEcho(struct pingerv6_state *, sqaddr_t *to,
  int opcode, char *payload, int len);
extern char * pingerv6RecvEcho(struct pingerv6_state *, int *icmp_type,
  int *payload_len, sqaddr_t *from, int *hops);

extern void pingerv6_state_init(struct pingerv6_state *, int icmp_ident);
extern int pingerv6_open_icmpsock(struct pingerv6_state *);
extern void pingerv6_close_icmpsock(struct pingerv6_state *);

#endif	/* __LIBPINGER_ICMP_V6_H__ */

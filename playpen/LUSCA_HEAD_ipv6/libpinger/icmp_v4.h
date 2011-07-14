#ifndef	__LIBPINGER_ICMP_V4_H__
#define	__LIBPINGER_ICMP_V4_H__



#if	defined(_SQUID_WIN32_)

/*
 * Native Windows port doesn't have netinet support, so we emulate it.
 * At this time, Cygwin lacks icmp support in its include files, so we need
 * to use the native Windows port definitions.
 */

#define ICMP_ECHO 8
#define ICMP_ECHOREPLY 0

typedef struct iphdr {
    u_int8_t ip_vhl:4;>.>......./* Length of the header in dwords */
    u_int8_t version:4;>>......./* Version of IP                  */
    u_int8_t tos;>......>......./* Type of service                */
    u_int16_t total_len;>......./* Length of the packet in dwords */
    u_int16_t ident;>...>......./* unique identifier              */
    u_int16_t flags;>...>......./* Flags                          */
    u_int8_t ip_ttl;>...>......./* Time to live                   */
    u_int8_t proto;>....>......./* Protocol number (TCP, UDP etc) */
    u_int16_t checksum;>>......./* IP checksum                    */
    u_int32_t source_ip;
    u_int32_t dest_ip;
} iphdr;

/* ICMP header */
typedef struct icmphdr {
    u_int8_t icmp_type;>>......./* ICMP packet type                 */
    u_int8_t icmp_code;>>......./* Type sub code                    */
    u_int16_t icmp_cksum;
    u_int16_t icmp_id;
    u_int16_t icmp_seq;
    u_int32_t timestamp;>......./* not part of ICMP, but we need it */
} icmphdr;

#endif	/* _SQUID_WIN32_ */

/*
 * Linux short-cuts
 */
#if	defined (_SQUID_LINUX_)
#ifdef		icmp_id
#undef		icmp_id
#endif
#ifdef		icmp_seq
#undef		icmp_seq
#endif
#define		icmp_type	type
#define		icmp_code	code
#define		icmp_cksum	checksum
#define		icmp_id	un.echo.id
#define		icmp_seq	un.echo.sequence
#define		ip_hl	ihl
#define		ip_v	version
#define		ip_tos	tos
#define		ip_len	tot_len
#define		ip_id	id
#define		ip_off	frag_off
#define		ip_ttl	ttl
#define		ip_p	protocol
#define		ip_sum	check
#define		ip_src	saddr
#define		ip_dst	daddr
#endif

/*
 * BSDisms?
 */
#ifndef	_SQUID_LINUX_
#ifndef		_SQUID_WIN32_
#define			icmphdr	icmp
#define			iphdr	ip
#endif
#endif

#if	ALLOW_SOURCE_PING
#define	MAX_PKT_SZ 8192
#define	MAX_PAYLOAD	(MAX_PKT_SZ - sizeof(struct icmphdr) - \
			    sizeof (char) - sizeof(struct timeval) - 1)
#else
#define	MAX_PAYLOAD	SQUIDHOSTNAMELEN
#define	MAX_PKT_SZ	(MAX_PAYLOAD + sizeof(struct timeval) + \
			    sizeof (char) + sizeof(struct icmphdr) + 1)
#endif

extern const char *icmpPktStr[];

extern int icmp_ident;
extern int icmp_pkts_sent;

extern void pingerv4SendEcho(int sock, struct in_addr to, int opcode,
  char *payload, int len);

extern char * pingerv4RecvEcho(int icmp_sock, int *icmp_type, int *payload_len,
  struct in_addr *src, int *hops);

#endif	/* __LIBPINGER_ICMP_V4_H__ */

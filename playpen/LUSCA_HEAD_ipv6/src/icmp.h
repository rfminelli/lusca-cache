#ifndef	__LUSCA_ICMP_H__
#define	__LUSCA_ICMP_H__

extern void icmpOpen(void);
extern void icmpClose(void);
extern void icmpSourcePing(sqaddr_t *to, const icp_common_t *, const char *url);
extern void icmpDomainPing(sqaddr_t *to, const char *domain);

#endif

#ifndef	__LIBIAPP_COMM_IPS_H__
#define	__LIBIAPP_COMM_IPS_H__


extern int comm_ips_bind(int fd, struct in_addr addr, u_short port);
extern void comm_ips_keepCapabilities(void);
extern void comm_ips_restoreCapabilities(void);

#endif

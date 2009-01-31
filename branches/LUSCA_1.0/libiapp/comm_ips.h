#ifndef	__LIBIAPP_COMM_IPS_H__
#define	__LIBIAPP_COMM_IPS_H__


extern int comm_ips_bind(int fd, sqaddr_t *a);
extern void comm_ips_keepCapabilities(void);
extern void comm_ips_restoreCapabilities(void);

#endif

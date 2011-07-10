#ifndef	__LIBIAPP_GLOBALS_H__
#define	__LIBIAPP_GLOBALS_H__

#define MAXHTTPPORTS                    128

extern int shutting_down;
extern int reconfiguring;
extern int opt_reuseaddr;
extern int iapp_tcpRcvBufSz;
extern const char * iapp_useAcceptFilter;
extern int NHttpSockets;        /* 0 */
extern int HttpSockets[MAXHTTPPORTS];
extern int theInIcpConnection4;  /* -1 */
extern int theOutIcpConnection4; /* -1 */
extern int theInIcpConnection6;  /* -1 */
extern int theOutIcpConnection6; /* -1 */
extern int iapp_incomingRate;
extern StatHist select_fds_hist;
extern int need_linux_tproxy;   /* 0 */

#endif

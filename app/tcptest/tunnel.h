#ifndef	__TCPTEST_TUNNEL_H__
#define	__TCPTEST_TUNNEL_H__

typedef struct {
    struct {
        int fd;
        int len;
        char *buf;
    } client, server;
    int connected;
    struct sockaddr_in peer;
} SslStateData;

extern void sslStart(int fd, struct sockaddr_in peer);


#endif

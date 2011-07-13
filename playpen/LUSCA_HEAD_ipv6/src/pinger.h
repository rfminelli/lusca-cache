#ifndef	__LUSCA_PINGER_H__
#define	__LUSCA_PINGER_H__

#define PINGER_PAYLOAD_SZ 8192 

/*
 * Use sockaddr_storage; it's big enough to
 * store IPv4 and IPv6 addresses.
 */

struct _pingerEchoData {
    struct sockaddr_storage to;
    unsigned char opcode;
    int psize;
    char payload[PINGER_PAYLOAD_SZ];
};

struct _pingerReplyData {
    struct sockaddr_storage from;
    unsigned char opcode;
    int rtt;
    int hops;
    int psize;
    char payload[PINGER_PAYLOAD_SZ];
};

typedef struct _pingerEchoData pingerEchoData;
typedef struct _pingerReplyData pingerReplyData;


#endif

#ifndef	__LIBSQBGP_BGP_PACKET_H__
#define	__LIBSQBGP_BGP_PACKET_H__

extern int bgp_msg_len(const char *buf, int len);
extern int bgp_msg_type(const char *buf, int len);
extern int bgp_msg_isvalid(const char *buf, int len);
extern int bgp_msg_complete(const char *buf, int len);

/* XXX eww? */
struct _bgp_instance;

extern int bgp_send_keepalive(struct _bgp_instance *bi, int fd);
extern int bgp_send_hello(struct _bgp_instance *bi, int fd, unsigned short asnum, short hold_time, struct in_addr bgp_id);

extern int bgp_decode_message(struct _bgp_instance *bi, int fd, const const char *buf, int len);

#endif

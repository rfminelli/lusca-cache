#ifndef	__LIBSQBGP_BGP_PACKET_H__
#define	__LIBSQBGP_BGP_PACKET_H__

extern int bgp_msg_len(const char *buf, int len);
extern int bgp_msg_type(const char *buf, int len);
extern int bgp_msg_isvalid(const char *buf, int len);
extern int bgp_msg_complete(const char *buf, int len);

/* XXX eww? */
struct _bgp_instance;

/*
 * XXX can we get away with -not- passing in the bgp_instance here?
 * XXX now that the RIB related stuff can be done elsewhere?
 */
extern int bgp_send_keepalive(struct _bgp_instance *bi, int fd);
extern int bgp_send_hello(struct _bgp_instance *bi, int fd, unsigned short asnum, short hold_time, struct in_addr bgp_id);

extern int bgp_handle_open(struct _bgp_instance *bi, int fd, const char *buf, int len);
extern int bgp_handle_update(struct _bgp_instance *bi, int fd, const char *buf, int len);
extern int bgp_handle_notification(struct _bgp_instance *bi, int fd, const char *buf, int len);
extern int bgp_handle_keepalive(int fd, const char *buf, int len);

#endif

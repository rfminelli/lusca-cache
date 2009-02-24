#ifndef	__LIBSQBGP_BGP_PACKET_H__
#define	__LIBSQBGP_BGP_PACKET_H__

extern int bgp_msg_len(const char *buf, int len);
extern int bgp_msg_type(const char *buf, int len);
extern int bgp_msg_isvalid(const char *buf, int len);
extern int bgp_msg_complete(const char *buf, int len);

/* XXX eww? */
struct _bgp_instance;

struct _bgp_update_state {
        u_int8_t aspath_type;
        u_int8_t aspath_len;
        u_short *aspaths;
        int origin;
        int nlri_cnt;
        struct in_addr *nlri;
        u_int8_t *nlri_mask;
        int withdraw_cnt;
        struct in_addr *withdraw;
        u_int8_t *withdraw_mask;
        struct in_addr nexthop;
};
typedef struct _bgp_update_state bgp_update_state_t;

/*
 * XXX can we get away with -not- passing in the bgp_instance here?
 * XXX now that the RIB related stuff can be done elsewhere?
 */
extern int bgp_send_keepalive(struct _bgp_instance *bi, int fd);
extern int bgp_send_hello(struct _bgp_instance *bi, int fd, unsigned short asnum, short hold_time, struct in_addr bgp_id);

extern int bgp_handle_open(struct _bgp_instance *bi, int fd, const char *buf, int len);
extern int bgp_handle_update(const char *buf, int len, bgp_update_state_t *us);
extern int bgp_handle_notification(struct _bgp_instance *bi, int fd, const char *buf, int len);
extern int bgp_handle_keepalive(int fd, const char *buf, int len);

extern void bgp_free_update(bgp_update_state_t *us);


#endif

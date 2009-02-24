#ifndef	__LIBSQBGP_BGP_PACKET_H__
#define	__LIBSQBGP_BGP_PACKET_H__

extern int bgp_msg_len(const char *buf, int len);
extern int bgp_msg_type(const char *buf, int len);
extern int bgp_msg_isvalid(const char *buf, int len);
extern int bgp_msg_complete(const char *buf, int len);

struct _bgp_open_state {
	u_int8_t version;
	u_int16_t asn;
	u_int16_t hold_timer;
	struct in_addr bgp_id;
	u_int8_t parm_len;
};
typedef struct _bgp_open_state bgp_open_state_t;

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

extern int bgp_send_keepalive(int fd);
extern int bgp_send_hello(int fd, unsigned short asnum, short hold_time, struct in_addr bgp_id);

extern int bgp_handle_open(const char *buf, int len, bgp_open_state_t *os);
extern void bgp_free_open(bgp_open_state_t *os);

extern int bgp_handle_update(const char *buf, int len, bgp_update_state_t *us);
extern void bgp_free_update(bgp_update_state_t *us);

extern int bgp_handle_notification(int fd, const char *buf, int len);
extern int bgp_handle_keepalive(int fd, const char *buf, int len);



#endif

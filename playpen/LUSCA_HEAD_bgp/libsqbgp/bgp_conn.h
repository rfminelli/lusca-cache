#ifndef	__BGPCONN_H__
#define	__BGPCONN_H__

struct _bgp_conn {
        bgp_instance_t bi;
        struct in_addr rem_ip;
        u_short rem_port;
        int fd;
        int keepalive_event;
};

typedef struct _bgp_conn bgp_conn_t;

bgp_conn_t * bgp_conn_create(void);
void bgp_conn_connect_wakeup(void *data);
void bgp_conn_connect_sleep(bgp_conn_t *bc);

void bgp_conn_send_keepalive(void *data);
void bgp_conn_destroy(bgp_conn_t *bc);
void bgp_conn_close_and_restart(bgp_conn_t *bc);
void bgp_conn_begin_connect(bgp_conn_t *bc);

#endif

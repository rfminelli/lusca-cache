#ifndef	__LIBSQBGP_BGP_CORE_H__
#define	__LIBSQBGP_BGP_CORE_H__

#define	BGP_RECV_BUF	4096

typedef enum {
	BGP_NONE = 0,
	BGP_IDLE,
	BGP_CONNECT,
	BGP_ACTIVE,
	BGP_OPENSENT,
	BGP_OPENCONFIRM,
	BGP_ESTABLISHED,
} bgp_fsm_t;

struct _bgp_instance {

	bgp_fsm_t state;
	struct {
		char buf[BGP_RECV_BUF];
		int bufofs;
	} recv;

	u_short hold_time;		/* Calculated from local and remote hold times */

	struct {
		u_short asn;
		u_short hold_timer;
		struct in_addr bgp_id;
	} lcl;

	struct {
		u_short asn;
		u_short hold_timer;
		struct in_addr bgp_id;
		u_int8_t version;
	} rem;

	/* The AS path table */

	/* The RIB */
	bgp_rib_head_t rn;
};

typedef struct _bgp_instance bgp_instance_t;

void bgp_create_instance(bgp_instance_t *bi);
void bgp_destroy_instance(bgp_instance_t *bi);

void bgp_set_lcl(bgp_instance_t *bi, struct in_addr bgp_id, u_short asn, u_short hold_time);
void bgp_set_rem(bgp_instance_t *bi, u_short asn);

void bgp_active(bgp_instance_t *bi);
int bgp_read(bgp_instance_t *bi, int fd);
void bgp_close(bgp_instance_t *bi);

#endif

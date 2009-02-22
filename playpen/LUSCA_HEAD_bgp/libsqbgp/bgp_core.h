#ifndef	__LIBSQBGP_BGP_CORE_H__
#define	__LIBSQBGP_BGP_CORE_H__

#define	BGP_RECV_BUF	4096

struct _bgp_instance {
	struct {
		char buf[BGP_RECV_BUF];
		int offset;
	} recv;

	u_short hold_time;		/* Calculated from local and remote hold times */

	struct {
		u_short asn;
		u_short hold_time;
		sqaddr_t addr;
	} lcl, rem;

	/* The AS path table */

	/* The RIB */
};

typedef struct _bgp_instance bgp_instance_t;

void bgp_create_instance(bgp_instance_t *bi);
void bgp_destroy_instance(bgp_instance_t *bi);


#endif

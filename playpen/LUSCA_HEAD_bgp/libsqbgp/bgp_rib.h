#ifndef	__LIBSQBGP_BGP_RIB_H__
#define	__LIBSQBGP_BGP_RIB_H__

struct _bgp_aspath {
	int refcnt;
	int pathlen;
	u_short *e;
};

#endif

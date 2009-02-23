#ifndef	__LIBSQBGP_BGP_RIB_H__
#define	__LIBSQBGP_BGP_RIB_H__

struct _bgp_rib_head {
	radix_tree_t	*rh;
	int num_prefixes;
};
typedef struct _bgp_rib_head bgp_rib_head_t;

struct _bgp_rib_aspath {
	u_short	origin_as;
};
typedef struct _bgp_rib_aspath bgp_rib_aspath_t;

extern int bgp_rib_match_host(bgp_rib_head_t *head, struct in_addr addr);
extern int bgp_rib_match_net(bgp_rib_head_t *head, struct in_addr addr, int masklen);
extern int bgp_rib_add_net(bgp_rib_head_t *head, struct in_addr addr, int masklen, u_short origin_as);
extern int bgp_rib_del_net(bgp_rib_head_t *head, struct in_addr addr, int masklen);
extern void bgp_rib_init(bgp_rib_head_t *head);
extern void bgp_rib_destroy(bgp_rib_head_t *head);
extern void bgp_rib_clean(bgp_rib_head_t *head);

#endif

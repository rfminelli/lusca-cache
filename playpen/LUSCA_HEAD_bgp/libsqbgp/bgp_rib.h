#ifndef	__LIBSQBGP_BGP_RIB_H__
#define	__LIBSQBGP_BGP_RIB_H__

struct _bgp_rib_head {
	radix_tree_t	*rh;
	int num_prefixes;
        u_short rib_genid;		/* A sequence number which is incremented each time a new BGP connection is established */
};
typedef struct _bgp_rib_head bgp_rib_head_t;

struct _bgp_rib_aspath {
	u_short	origin_as;
	u_short rib_genid;
	struct {
		u_short hist;			/* Is this route entry "historical" for the current generation? */
	} flags;
};
typedef struct _bgp_rib_aspath bgp_rib_aspath_t;

extern int bgp_rib_match_host(bgp_rib_head_t *head, struct in_addr addr);
extern int bgp_rib_match_net(bgp_rib_head_t *head, struct in_addr addr, int masklen);
extern int bgp_rib_add_net(bgp_rib_head_t *head, struct in_addr addr, int masklen, u_short origin_as);
extern int bgp_rib_del_net(bgp_rib_head_t *head, struct in_addr addr, int masklen);
extern int bgp_rib_mark_historical(bgp_rib_head_t *head, struct in_addr addr, int masklen);
extern void bgp_rib_init(bgp_rib_head_t *head);
extern void bgp_rib_destroy(bgp_rib_head_t *head);
extern void bgp_rib_clean(bgp_rib_head_t *head);
extern void bgp_rib_bump_genid(bgp_rib_head_t *head);

#endif

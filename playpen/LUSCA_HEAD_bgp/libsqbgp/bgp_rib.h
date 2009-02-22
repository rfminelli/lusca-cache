#ifndef	__LIBSQBGP_BGP_RIB_H__
#define	__LIBSQBGP_BGP_RIB_H__

extern int bgp_rib_match_ip(struct squid_radix_node_head *head, void *data, struct in_addr addr);
extern void bgp_rib_init(struct squid_radix_node_head **head);
extern void bgp_rib_destroy(struct squid_radix_node_head *head);
extern void bgp_rib_clean(struct squid_radix_node_head *head);
extern int bgp_rib_add_net(struct squid_radix_node_head *head, char *as_string, int as_number);

#endif

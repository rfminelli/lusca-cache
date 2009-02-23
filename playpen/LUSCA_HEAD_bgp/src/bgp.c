#include "squid.h"

#include "../libsqbgp/radix.h"
#include "../libsqbgp/bgp_packet.h"
#include "../libsqbgp/bgp_rib.h"
#include "../libsqbgp/bgp_core.h"
#include "../libsqbgp/bgp_conn.h"

static bgp_conn_t *bc = NULL;

void
bgpStart(void)
{
	/* Is it configured? If not, don't bother. */
	if (! Config.bgp.enable)
		return;

	/* Do we have a BGP instance? If not, create one */
	if (bc == NULL) {
		bc = bgp_conn_create();
		bgp_set_lcl(&bc->bi, Config.bgp.local_ip, Config.bgp.local_as, 60);
		bgp_set_rem(&bc->bi, Config.bgp.remote_as);
		memcpy(&bc->rem_ip, &Config.bgp.remote_ip, sizeof(bc->rem_ip));
		bc->rem_port = 179;
		/* Kick it alive */
		bgp_conn_begin_connect(bc);
	}

}

void
bgpReconfigure(void)
{
	/* Only restart the BGP session if the configuration doesn't match the live one */
	/* XXX for now, since I'm lazy */
	bgpShutdown();
	bgpStart();
}

void
bgpShutdown(void)
{
	if (bc == NULL)
		return;

	bgp_conn_destroy(bc);
	bc = NULL;
}

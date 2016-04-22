# IPv6 #

This needs to be looked into in a little more depth, building on Amos' work in Squid-3 along with looking at how other network applications do it. It shouldn't be _that_ hard to get client-side IPv6 included.

Should combined v4/v6 sockets be supported, or just force sockets to be v4-only or v6-only?

## Client-only IPv6 ##

The stuff that needs doing, as I think about it:

  * In the support libraries
    * Support v4/v6 address information
    * ipv4/ipv6 socket creation
    * ipv6 connect()
    * ipv6 bind()
    * what else?

  * First cut - support v4/v6 support libraries but only "support" v4
    * assertions where the code assumes a v4 IP address at the moment?

  * In the Squid application itself
    * src6/dst6 ACL types
    * src/dst protocol types
    * all the places which build hashes based on the socket IP address - client\_db, pconn\_db, etc..
    * SNMP, both v4/v6 agent and v4/v6 OID modifications? (some OIDs use a v4 address as the key?)
    * Some headers? I'm not sure yet.
    * DNS, which probably should be migrated out into an external library, along with the ipcache/fqdncache caches.

## Squid IPv6 (not server-side!) ##

  * Um, X-Forwarded-For needs to worry about v6 IP addresses potentially showing up?
  * v4/v6 ICP support
  * v4/v6 HTCP support

## Squid server-side IPv6 ##

  * I'm not looking at this at the moment; there's a lot to do to act as a v4 

&lt;-&gt;

 v6 HTTP and FTP gateway.
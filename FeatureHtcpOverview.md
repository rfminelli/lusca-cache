# Introduction #

HTCP is an inter-cache control protocol. It includes much more information in the request than ICP (including request headers) and can be used to subscribe to various cache functions such as object creation and deletion.

# Configuration #

Squid/Lusca does not include HTCP support by default. It needs to be compiled with the configure option "--enable-htcp".

The default HTCP port is UDP/4327. This can be changed with the configuration options "htcp\_port".

There are two separate configuration options for HTCP Access control - "htcp\_access" and "htcp\_clr\_access". "http\_access" control general HTCP access; "htcp\_clr\_access" controls the HTCP CLR (Clear/Purge) access.

HTCP is configured on a peer by using "htcp" on the "cache\_peer" option for the given peers. CLR messages are forwarded to peers with the "htcp-forward-clr" option set.

If HTCP is to be configured for a given peer, change the ICP port to the HTCP port on the "cache\_peer" line or HTCP messages will be sent to the ICP port.

# Known Issues #

## HTCP request loops with HTCP\_CLR ##

HTCP CLR messages (ie, Clear) will be forwarded to all peers which are configured to forward CLR messages. There is however no loop detection in the HTCP codebase. It is therefore easy to create a loop between two peers which will bounce a CLR message between them.

There is no current workaround save for not configuring any CLR forwarding loops in the cache peer configuration.


# More Information #

  * [Wikipedia Article on HTCP](http://en.wikipedia.org/wiki/Hypertext_caching_protocol)
# Introduction #

Lusca supports client-side and server-side IPv4 address spoofing for a variety of platforms.

Please note that this documentation is intended as an overview. The authoritative source is the code and the code documentation.

# Background #

Squid has supported client-side interception for a number of years. The support includes a variety of possible code paths for each operating system and firewall type (IPFilter, IPFW, Netfilter, PF.) Recently, support for the Balabit TPROXY2 and TPROXY4 interception frameworks allow Squid to intercept both client and server facing traffic, presenting a (mostly!) seamless transparency to both sides of the connection.

The idea is being developed out in Lusca to provide transparent interception support to any application written which uses the Lusca network core.

# Components #

There are two main components. The first is intercepting connections from clients - ie, spoofing the server-side address. The second is originating outbound connections to upstream origin servers and intermedaries but using the original IP address of the client.

## Client connection interception ##

The client-side interception is currently performed in src/client\_side.c. The clientNatLookup() function is defined based on the compile-time interception mode selected. If none are selected, this falls back to imply calling getsockname() on the socket to determine the local IP address of the socket connection.

## Client-side address spoofing ##

This was introduced in TPROXY2 and TPROXY4. This API allows a separate, non-local IP address to be used when creating an outbound socket address. Again, this is compile-time dependent.

The original TPROXY2 support inserted specific code in src/forward.c to map a newly-created outbound socket to a different source address (using TPROXY2 specific options) before connecting it to a remote host.

The TPROXY4 support removed this special code and instead created a socket with a special comm option (COMM\_TRANSPARENT) which signaled to the comm layer to attempt the "source spoofing" ioctl(), which then allows the non-local IP address to be set via a subsequent call to bind().

Further changes to the Lusca codebase are aimed at creating a clear API for defining inbound and outbound transparently intercepted connections.

## Accepting new connections ##

From a socket level, the interception code from Squid-2 doesn't really treat intercepted connections any differently. A normal IPv4 socket is created, bind(), listen(), and accept() happen just like a non-interception method.

The majority of the differences lie in how the connection is treated. An intercepted connection requires Lusca/Squid to treat the request as if it were the origin server. This has subtle changes in the request URI and Host header requirements.

There are specific hacks which try to determine the original destination address. This is used when no specific Host: header is given.

Linux TPROXY4 changes this slightly. Since TPROXY4 specific rules are involved in the interception, a specific socket option is required (IP\_TRANSPARENT) before the bind() and listen() syscalls are called.

# Network topology implications #

Full transparency requires the proxy server to see both sides of the traffic flow. It will then redirect relevant packets making up the intercepted client and server connections through the correct sockets.

(TODO: flesh this out some more.)

# Using this in code #

## Accepting transparently intercepted connections from clients ##

  * Create a socket using comm\_open() with the relevant comm flags. If the connection needs to handle client-spoofed requests, set COMM\_TPROXY\_LCL. If the connection is just doing normal server address spoofing and no client address spoofing, no special flag is (currently) needed. (Note - this will change in the future for the sake of completeness.)
  * Call comm\_listen()
  * Setup an FD handler using commSetSelect() to handle incoming requests

## Determining the original destination server IP address ##

This is very system specific:

  * ipfw (FreeBSD/NetBSD/OpenBSD?): the ipfw code overrides the socket local endpoint address with the original destination; getsockname() thus returns the original destination
  * Linux Netfilter: An IP socket option (SO\_ORIGINAL\_DST) is called to determine the original destination
  * PF - an ioctl is performed on an open filedescriptor to /dev/pf - DIOCNATLOOK
  * IPFilter - simiarly to PF, an ioctl is performed on an open filedescriptor to an ipfilter device.

This is all done in client\_side.c:clientNatLookup() - it is not yet available as a generalised API for determining the original destination.

## Using a non-local IP address on an outbound connection ##

  * Create a socket using comm\_open() with the comm flag COMM\_TPROXY\_REM set.
  * COMM\_TPROXY\_REM allows the local address specified in comm\_open() to be non-local.
  * Complete the connection as normal.

# Implementation notes #

## Server-side interception modules ##

## Client-side spoofing modules ##

## Linux TPROXY2 ##

## Linux TPROXY4 ##

## FreeBSD-current BIND\_ANY ##

## NetBSD/OpenBSD BIND\_ANY ##

# Future work #

## WCCPv2 integration ##

## Proxy bypass ##

## Non-HTTP protocols ##

## IPv6 support ##
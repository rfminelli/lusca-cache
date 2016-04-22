# Introduction #

This feature allows the administrator to delay forwarding specific POST requests until all or a certain amount of POST data has been received.


# Available in #

  * Squid-2.HEAD
  * CACHEBOY\_HEAD from 17-Jan-2009

# Details #

The primary motivation for this feature is to prevent a back-end application server from having too many idle POST connections open whilst a slow client transfers the POST request. For example, a Java application server with a high per-connection cost (generally memory) may scale poorly with a few thousand open POST connections with the request body coming in slowly.

Squid therefore acts as a sort of "buffer".

By default, requests are forwarded immediately. An ACL directive (request\_body\_delay\_forward\_size) can be used in conjuction with HTTP request ACLs (ie, ACLs matching on the HTTP request) to delay forwarding the request until one of two criteria are met:

  * The entire POST body has been received, or
  * The entire POST body has not been received but enough data (as specified in the ACL directive) has been received.

# Examples #

(To Be Done)

# Warnings #

There is currently no checking on memory use. You therefore should be aware that the feature may cause Squid/Cacheboy to use excessive amounts of system RAM (going into swap!) when too much RAM is being used for buffering incoming requests.

The solution would be a little logic to track how much RAM is being used as socket buffers and start throttling back on the buffered connections a little.
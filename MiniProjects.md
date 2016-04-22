# Mini Projects #

These are little projects which need doing that should be doable for a newcomer to the project.

## Error Pages ##

Better error pages are needed.

My current playpen: http://code.google.com/p/cacheboy/source/browse/playpen/errors/

The basic outline:

  * Figure out a "good" CSS layout for the error documents
  * Build one or two example CSS files to demonstrate the flexibility of the error page layout
  * Begin converting the error pages into the "new world order"

Notes!

  * Squid/Cacheboy -can- serve static content (as icons!) but its a terrible hack. The initial error pages will need the CSS included in-line rather than referencing an external CSS document. This should be done anyway as serving error pages can happen during "errors" and it'd be unfortunate if something required on a error page generated an error..

## Remove the stdio calls ##

I'd start by looking at the calls to inet\_ntoa(), and thinking about how to remove them except where the IP addresses are being **displayed**. The inet\_ntoa0 strings appear in a number of hash table key building places ; the hash code doesn't take a length parameter and expects a NUL terminated value.

So I'd do this:

  * Fix the hash table code to pass a ptr/key, not just ptr
  * and make sure it works with non-NUL terminated data -and- data with NULs inline
  * (Which will require significant codebase modifications (but not intrusive changes) all over the place!)
  * Then, when this is done, change a few places which use inet\_ntoa() to generate a key - starting with clientdb and persistent connections
  * Rebenchmark!

## Forward port the Squid-2.6 iCap support ##

I'd like to see the Squid-2.6 ICAP patch forward ported to Cacheboy. The integration into Squid-2 is a bit messy and the iCap stuff isn't fully compliant but it works well enough for most people and its a good place to start.

## IPv6 client support ##

Another thing I'd like to see is IPv6 client support. IPv6 server support is a lot more trickier.

The things to do for v6 client support which I've gleaned from the Squid-3 IPv6 effort and husni's Squid-2.6 IPv6 stuff:

  * A lot of sockaddr\_in and in\_addr's need to be fiddled to be something v4/v6 agnostic - the set of things which store client-related addresses, for example.
  * A "v4/v6" type (ie IPAddress in Squid-3) would be nice - just use the "right" type!
  * Various areas of the code which assume v4 need to be updated - client\_side, a little of http.c, perhaps store\_client, pconn, client\_db, snmp come to mind - these use the sockaddr/in\_addr directly or via inet\_ntoa() for various things (generating hashes to store data against, printing out to headers, etc.)
  * ACL changes - a src6/dst6 would be nice; a unified v4/v6 would be nicer but can come later
    * With this: an address family ACL (v4/v6) would also be nice
    * What about v4 in v6 addresses?
  * Comm changes - only really need to handle the accept() case correctly, as client-side support doesn't require any connect() support
  * fd changes - primarily to store the IPv6 address somewhere sensible
# Introduction #

This is a list of "stuff" that has changed and needs further testing to make sure there are no regressions.

  * The new Solaris Event Ports framework - new!
  * Helpers (both normal and parallel) - changed the pending request queues
  * AUFS - changed the pending read/write queues
  * HEAP replacement types - changed the queue used to shuffle objects around
  * FTP code - [revision 12657](https://code.google.com/p/lusca-cache/source/detail?r=12657) changed some of the command parsing stuff
  * intlist stuff - ASN, RTT and Method related ACLs.
  * The debugging stuff has changed slightly (as some debugging is now in libcore/); check to make sure file -and- syslog debugging for cache\_log works correctly.
  * TPROXY2 support - the TPROXY support has changed
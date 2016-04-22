# Overview #

Initially we treat the following sections, which are:

  * Debug message text should be easy understandable
  * Better debug level classification for less unecessary log spammimg
  * Config parser should generate better debug
  * -k reconfigure needs revision
  * Cache Peer Selection with better weight options


# Change Suggestions #

> ## Debug Levels ##

> The standard cache\_log as defined in squid.conf or syslog if started with -s is actually spammed by lot's of irrelevant text when debug\_options ALL,1 is set. The debug\_options N,1 should show only important messages which could lead to problems or are real problems. Oherwise, this important messages might get lost (unseen) between the less important and expensive messages.

> So far I suggest to raise the debug level of the following keywords from N,1 to N,2 or N,3

  * clientTryParseRequest
  * httpAccept
  * httpReadReply
  * httpAppendBody
  * helperHandleRead
  * parseHttpRequest

> Who then likes to see them again only needs to raise the debug level from

  * debug\_options ALL,1
> to
    * debug\_options ALL,2

> You might find inverted situations, where certain messages should appear as level 1 and are 2 or 3 or higher. Please post them as well.


> ## Debug Message Text ##

> Certain debug messages show up with wierd text, hard to understand or eventually wrong. As example one here:

  * Actually    debug(1, 1) ("Squid Cache (Version %s): Exiting normally.\n",

> but squid is already shutdown when this messages appears. Irrelevant, yes, but even so wrong, better would be:

  * New msg     debug(1, 1) ("WebCache (Version %s): Exited normally.\n",

> As you see, also the word Squid Cache is changed, into Web Cache or could be Lusca Web Cache.



# Feature Wishes #

  * option to disable unlinkd (or totally remove it?)
  * full-featured [http pipelining](http://en.wikipedia.org/wiki/HTTP_pipelining)
  * kb.c, gb.c to (very restricted) bignum
    * could be bit slower, but cleaner
    * int64\_t isn't enouth?
  * allow users to set numbers in any units in squid.conf (KB, MB, GB, TB!)
#summary Which Patches to integrate

# Introduction #

There's no nice, simple script that I can find to apply CVS patchsets (from cvsps) to Subversion repos previously setup with cvs2svn.

This page is just a placeholder to manually track patchsets that need merging into the Squid 'trunks'.

At some point a tool should be written to do this!

# Patchsets #

The initial import included all patchsets up to 12036.

## Stuff merged into trunk and then cross-merged into CACHEBOY\_PRE / CACHEBOY\_HEAD ##

  * 12037 - **committed**
  * 12038 - **committed**
  * 12039 - **committed**
  * 12040 - **committed**
  * 12041 - **committed**
  * 12042 - **committed**
  * 12043 - **committed**
  * 12044 - **committed**
  * 12045 - HEAD - minimum\_icp\_query\_timeout directive - **committed - rev 12616**
  * 12046 - HEAD - ZPH patch - **committed - rev 12617**
  * 12047 - SQUID\_2\_7 - [Bug #2296](https://code.google.com/p/lusca-cache/issues/detail?id=2296) - 100% CPU when fething corrupt peer digest - **committed - rev 12619**
  * 12048 - HEAD - [Bug #2296](https://code.google.com/p/lusca-cache/issues/detail?id=2296) - 100% CPU when fetching corrupt peer digest - **committed - rev 12620**
  * 12049 - SQUID\_2\_6 - [Bug #2296](https://code.google.com/p/lusca-cache/issues/detail?id=2296) - 100% CPU when fetching corrupt peer digest - **committed - rev 12621**
  * 12050 - SQUID\_2\_7 - [Bug #2208](https://code.google.com/p/lusca-cache/issues/detail?id=2208) - Document major HTTP/1.1 shortcomings - **committed - rev 12622**
  * 12051 - SQUID\_2\_7 - [Bug #2192](https://code.google.com/p/lusca-cache/issues/detail?id=2192) - Accelerator mode option cleanups - **committed - rev 12623**
  * 12052 - SQUID\_2\_7 - resolv.conf domain directive - **committed - rev 12625**
  * 12053 - SQUID\_2\_7 - minimum\_icp\_query\_timeout directive - **committed - rev 12626**
  * 12054 - SQUID\_2\_7 - TCP keepalive support - **committed - rev 12627**
  * 12055 - SQUID\_2\_7 - ZPH patch - **committed - rev 12628**
  * 12056 - SQUID\_2\_6 - resolv.conf domain directive - **committed - rev 12629**
  * 12057 - SQUID\_2\_6 - minimum\_icp\_query\_timeout directive - **committed - rev 12630**
  * 12058 - SQUID\_NT\_2\_6 - Merged changes from SQUID\_2\_6 - **committed - rev 12631**
  * 12059 - SQUID\_NT\_2\_7 - Merged changes from SQUID\_2\_7 - **committed - rev 12632**
  * 12060 - HEAD - Windows port: Return the effective system pagesize in getpagesize() - **committed - rev 12633**
  * 12061 - HEAD - Change/release notes for 2.6/2.7 - **committed - rev 12635**
  * 12062 - SQUID\_2\_6 - Squid 2.6.STABLE20 - **committed - rev 12636**
  * 12063 - SQUID\_2\_6 - Tag Z-SQUID\_NT\_2\_6\_merge-new\_SQUID\_2\_6 - bootstrapped - **committed - rev 12637**
  * 12064 - SQUID\_2\_7 - Documentation update - **committed - rev 12638**
  * 12065 - SQUID\_NT\_2\_6 - merged changes from SQUID\_2\_6 - **committed - rev 12639**
  * 12066 - HEAD - Windows related DNS/host changes - **committed - rev 12640**
  * 12067 - SQUID\_NT\_2\_6 - bootstrapped - **committed - rev 12641**
  * 12068 - SQUID\_NT\_2\_6 - Revert unwanted changes - **committed - rev 12642**
  * 12069 - HEAD - net\_db typo - **committed - rev 12643**
  * 12070 - HEAD - [Bug 2329](https://code.google.com/p/lusca-cache/issues/detail?id=329): Range header ignored on HIT - **committed - rev 12686**
  * 12071 - SQUID\_2\_7 - Windows port: Return the effective system pagesize in getpagesize() - **committed - rev 12687**
  * 12072 - SQUID\_2\_7 - Windows port: Add support for the Windows machine DNS domain, and also automatically derived default domain - **committed - rev 12688**
  * 12073 - SQUID\_2\_7 - [Bug 2329](https://code.google.com/p/lusca-cache/issues/detail?id=329): Range header ignored on HIT - **committed - rev 12689**
  * 12074 - SQUID\_2\_6 - Windows port: Return the effective system pagesize in getpagesize() - **committed - rev 12691**
  * 12075 - SQUID\_2\_6 - Windows port: Add support for the Windows machine DNS domain, and also automatically derived default domain - **committed - rev 12692**
  * 12076 - SQUID\_2\_6 - [Bug 2329](https://code.google.com/p/lusca-cache/issues/detail?id=329): Range header ignored on HIT - **committed - rev 12693**
  * 12077 - HEAD - Documentation update preparing 2.6.STABLE20 - **committed - rev 12694**
  * 12078 - SQUID\_2\_6 - Documentation update preparing 2.6.STABLE20 - **committed - rev 12696**
  * 12079 - SQUID\_2\_7 - 2.6.STABLE20 - **committed - rev 12697**
  * 12080 - HEAD - Corrected function names in debugging statements - **committed - rev 12698**
  * 12081 - HEAD - Corrected function names in debugging statements - **committed - rev 12699**
  * 12082 - SQUID\_NT\_2\_6 - Merged changes from SQUID\_2\_6 - **committed - rev 12701**
  * 12083 - SQUID\_NT\_2\_7 - Merged changes from SQUID\_2\_7 - **committed - rev 12702**
  * 12084 - HEAD - Windows port: The Window support is no more "EXPERIMENTAL" since long time ... - **committed - rev 12703**
  * 12085 - HEAD - Import reconfigure cachemgr action from nt Devel branch - **committed - rev 12704**
  * 12086 - HEAD - Removed the advertisement clause from BSD license - **committed - rev 12705**
  * 12087 - HEAD - Windows port: allow build of squid\_radius\_auth on Windows - **committed - rev 12706**
  * 12088 - HEAD - Windows port: clean no more needed type casts into dns code - **committed - rev 12707**
  * 12089 - HEAD - Windows port: add support for dirent-style functions - **committed - rev 12709**
  * 12090 - HEAD - Windows port: add support for getopt function - **committed - rev 12710**
  * 12091 - HEAD - bootstrapped - **committed - rev 12713**
  * 12092 - HEAD - [Bug #2306](https://code.google.com/p/lusca-cache/issues/detail?id=2306): Segfault on mgr:active\_requests after delay pool statistics changes - **committed - rev 12714**
  * 12093 - HEAD - [Bug #219](https://code.google.com/p/lusca-cache/issues/detail?id=219): Reassign delay pools after reconfigure - **committed - rev 12715**
  * 12094 - HEAD - Make --with-large-files try to build 64-bit if possible - **committed - rev 12716**
  * 12095 - HEAD - bootstrapped - **committed - rev 12717**
  * 12096 - HEAD - Windows port: clean last remains of no more needed type casts into dns code - **committed - rev 12718**
  * 12097 - HEAD - [BUg #2328](https://code.google.com/p/lusca-cache/issues/detail?id=2328): Slow filedescriptor leak when update\_headers on - **committed - rev 12719**
  * 12098 - SQUID\_2\_7 - [BUg #2328](https://code.google.com/p/lusca-cache/issues/detail?id=2328): Slow filedescriptor leak when update\_headers on - **committed - 12720**
  * 12099 - SQUID\_2\_7 - Corrected function names in debugging statements - **committed - 12721**
  * 12100 - SQUID\_2\_7 - Windows port updates - **committed - 12723**
  * 12101 - SQUID\_2\_7 - This patch add the reconfigure restricted action to Cache Manager. - **committed - 12724**
  * 12102 - SQUID\_2\_7 - Removed advertisement clause from BSD licensed sources - **committed - 12725**
  * 12103 - SQUID\_2\_7 - Make --with-large-files try to build 64-bit if possible - **committed - 12726**
  * 12104 - SQUID\_2\_7 - bootstrapped - **committed - 12727**
  * 12105 - HEAD - Fix [bug #2317](https://code.google.com/p/lusca-cache/issues/detail?id=2317): assertion failed: store.c:888: "size + state->buf\_offset <= state->buf\_size" - **committed - 12728**
  * 12106 - SQUID\_NT\_2\_7 - Removed files superseeded by new HEAD sources - **committed - 12746**
  * 12107 - SQUID\_NT\_2\_7 - Merged changes from SQUID\_2\_7 - **committed - 12748**
  * 12108 - SQUID\_NT\_2\_7 - Updated include file for new dirent.c and getopt.c - **committed - 12749**
  * 12109 - SQUID\_NT\_2\_7 - Added squid\_radius\_auth helper to Visual Studio Project - **committed - 12750**
  * 12110 - SQUID\_NT\_2\_7 - Updated the Visual Studio Project for dirent.c and getopt.c changes - **committed - 12751**
  * 12111 - SQUID\_NT\_2\_7 - Bootstrapped - **committed - 12752**
  * 12112 - SQUID\_NT\_2\_7 - Remove no more needed include file - **committed - 12753**
  * 12113 - HEAD - Windows port: allow build of squid\_session on Windows - **committed - 12754**
  * 12114 - SQUID\_2\_7 - Windows port: allow build of squid\_session on Windows - **committed - 12755**
  * 12115 - SQUID\_NT\_2\_7 - Merged changes from SQUID\_2\_7 - **committed - 12756**
  * 12116 - SQUID\_NT\_2\_7 - Added squid\_session to Visual Studio Project - **committed - 12757**
  * 12117 - HEAD - [Bug #2335](https://code.google.com/p/lusca-cache/issues/detail?id=2335): fix error return in store\_client.c leading to NULL dereference - **committed - 12758**
  * 12118 - HEAD - Put XMIN back for the case that sz > -1; we still dont' want to send > copy\_size data over! - **committed - 12764**

  * 12119 - HEAD - Windows port: Added new mswin\_check\_ad\_group external ACL helper - **committed 12873**
  * 12120 - HEAD - Updated mswin\_check\_lm\_group documentation - **committed 12874**
  * 12121 - HEAD - Fixed typo in mswin\_check\_ad\_group documentation - **committed 12875**
  * 12122 - HEAD - Fix defaultsite=host:port accelerator mode again... - **committed 12876**
  * 12123 - HEAD - Correct Via response header when seeing an HTTP/0.9 response - **committed 12877**
  * 12124 - HEAD - Bootstrapped **Needs to be hand-extracted; cvsps broke** - **committed - 12878**
  * 12125 - HEAD - Forgotten to add Makefile.in to mswin\_check\_ad\_group - **committed - 12879**
  * 12126 - HEAD - [Bug #2350](https://code.google.com/p/lusca-cache/issues/detail?id=2350): Memory allocation problem in restoreCapabilities() - **committed - 12880**
  * 12127 - HEAD - Commit some basic docs for the store URL rewriter. - **committed - 12881**
  * 12128 - HEAD - [Bug #1955](https://code.google.com/p/lusca-cache/issues/detail?id=1955): Clarify refresh\_pattern override-expire option - **committed - 12882**

**At this point I've decided to not bother mirroring patches from anything other than HEAD**

  * 12129 - SQUID\_2\_7 - Windows port: Added new mswin\_check\_ad\_group external ACL helper
  * 12130 - SQUID\_2\_7 - Updated mswin\_check\_lm\_group documentation
  * 12131 - SQUID\_2\_7 - Fix defaultsite=host:port accelerator mode again...
  * 12132 - SQUID\_2\_7 - Correct Via response header when seeing an HTTP/0.9 response
  * 12133 - SQUID\_2\_7 - [Bug #2350](https://code.google.com/p/lusca-cache/issues/detail?id=2350): Memory allocation problem in restoreCapabilities()
  * 12134 - SQUID\_2\_7 - some basic docs for the store URL rewriter.
  * 12135 - SQUID\_2\_7 - [Bug #1955](https://code.google.com/p/lusca-cache/issues/detail?id=1955): Clarify refresh\_pattern override-expire option
  * 12136 - SQUID\_2\_7 - [Bug #219](https://code.google.com/p/lusca-cache/issues/detail?id=219): Reassign delay pools after reconfigure
  * 12137 - HEAD - [Bug #2223](https://code.google.com/p/lusca-cache/issues/detail?id=2223): flexible handling of x-forwarded-for - **committed - 12890**
  * 12138 - SQUID\_2\_7 - [Bug #2223](https://code.google.com/p/lusca-cache/issues/detail?id=2223): flexible handling of x-forwarded-for
  * 12139 - SQUID\_2\_7 - documentation update
  * 12140 - SQUID\_2\_7 - Bootstrapped **cvsps broke again**
  * 12141 - HEAD - Dist mswin\_ad\_group - **committed - 12891**
  * 12142 - HEAD - mswin\_ad\_group was already disted - **committed - 12892**
  * 12143 - SQUID\_2\_7 - Resolve merge conflict in mswin\_ad\_group
  * 12144 - SQUID\_2\_7 - 2.7.STABLE1
  * 12145 - SQUID\_2\_7 - Bootstrapped (SQUID\_2\_7\_STABLE1 ?)
  * 12146 - HEAD - Merged 2.7 doc updates - **committed - 12893**
  * 12147 - SQUID\_2\_7 - Resolve merge conflict in [Bug #219](https://code.google.com/p/lusca-cache/issues/detail?id=219): Reassign delay pools after reconfigure
  * 12148 - SQUID\_2\_7 - Backout the patch for [Bug #1893](https://code.google.com/p/lusca-cache/issues/detail?id=1893): Variant invalidation on PURGE and HTCP CLR
  * 12149 - SQUID\_2\_7 - Merged changes from SQUID\_2\_7 **cvsps broke again!**
  * 12150 - SQUID\_NT\_2\_7 - Added mswin\_check\_ad\_group to Visual Studio Project
  * 12151 - SQUID\_NT\_2\_7 - Bootstrapped

**Squid-2 HEAD patches only**

  * 12152 - HEAD - [Bug #2360](https://code.google.com/p/lusca-cache/issues/detail?id=2360): Move the SSL options before https\_port so it gets inherited proper - **committed - 12894**
  * 12153 - HEAD - [Bug #2350](https://code.google.com/p/lusca-cache/issues/detail?id=2350): Linux Capabilities version mismatch causing startup crash on newer kernels - **committed - 12895**
  * 12154 - HEAD - Correct Linux capabilities version check - **committed - 12896**
  * 12155 - HEAD - cat - **committed - 12897**
  * 12156 - HEAD - Fix build error on Windows: in\_addr\_t is not available. - **committed - 12898**
  * 12157 - HEAD - Windows port: add support for crypt function - **committed - 12899**
  * 12158 - HEAD - Updated Windows specific 2.7 release notes - **committed - 12900**
  * 12159 - HEAD - [Bug #2283](https://code.google.com/p/lusca-cache/issues/detail?id=2283): Properly abort invalid/truncated messages - **committed - 12901**
  * 12164 - HEAD - Preparing for 2.7.STABLE2 - **committed - 12902**
  * 12167 - HEAD - Bootstrapped - **committed - 12904**
  * 12170 - HEAD - Cosmetic cleanup of Windows specific release notes - **committed - 12905**
  * 12173 - HEAD - forced commit for rev 12155 log message - ignored
  * 12174 - HEAD - Fix [Bug #2366](https://code.google.com/p/lusca-cache/issues/detail?id=2366): close ipc-created filedescriptors correctly on rotate. - **committed - 12906**
  * 12177 - HEAD - Windows port: fix build error on Cygwin - **committed - 12907**
  * 12179 - HEAD - Windows port: configure enhancements on MinGW and Cygwin - **committed - 12908**
  * 12180 - HEAD - Windows port: always shutdown winsocks on program termination - **committed - 12909**
  * 12181 - HEAD - delay\_body\_max\_size is in 2.8 only. - **committed - 12910**
  * 12182 - HEAD - Bootstrapped - **committed - 12911**
  * 12183 - HEAD - Windows port: add option for control of IP address changes notification in squid.conf - **committed - 12912**
  * 12184 - HEAD - [Bug #2283](https://code.google.com/p/lusca-cache/issues/detail?id=2283): Properly abort invalid/truncated messages - **committed - 12913**
  * 12185 - HEAD - Debug tool to run named events NOW - **committed - 12914**
  * 12186 - HEAD - [Bug #2372](https://code.google.com/p/lusca-cache/issues/detail?id=2372): Gracefully handle logfile permission errors on non-fatal logs - **committed - 12915**
  * 12193 - HEAD - ChangeLog update - **committed - 12916**
  * 12198 - HEAD - Fix build error on Solaris using gcc and --with-large-files - **committed - 12917**
  * 12202 - HEAD - Bootstrapped - **committed - 12918**
  * 12204 - HEAD - Allow 64 bit build on Solaris using gcc after fix of --with-large-files error - **committed - 12919**
  * 12206 - HEAD - Bootstrapped - **committed - 12920**
  * 12208 - HEAD - Make PEER\_TCP\_MAGIC\_COUNT configurable - **committed - 12921**
  * 12209 - HEAD - [Bug #2377](https://code.google.com/p/lusca-cache/issues/detail?id=2377): Include the new connect-fail-limit= cache\_peer option in config dumps - **committed - 12922**
  * 12212 - HEAD - [Bug #2192](https://code.google.com/p/lusca-cache/issues/detail?id=2192): http\_port ... vport broken by recent changes in how accelerator mode deals with port numbers - **committed - 12923**
  * 12213 - HEAD - [Bug #2241](https://code.google.com/p/lusca-cache/issues/detail?id=2241): weights not applied properly in round-robin peer selection - **committed - 12924**
  * 12214 - HEAD - Backout the patch for [Bug #1893](https://code.google.com/p/lusca-cache/issues/detail?id=1893): Variant invalidation on PURGE and HTCP CLR - **commited - 12925**
  * 12215 - HEAD - Off by one error in DNS label decompression could cause valid DNS messages to be rejected - **committed - 12926**
  * 12216 - HEAD - logformat docs contain extra whitespace - **committed - 12927**
  * 12217 - HEAD - Reject ridiculously large ASN.1 lengths - **committed - 12928**
  * 12218 - HEAD - Fix SNMP reporting of counters with a value > 0xFF80000 - **committed - 12929**
  * 12226 - HEAD - Correct spelling of WCCPv2 dst\_port\_hash to match the source - **committed - 12930**
  * 12228 - HEAD - Documentation update preparing for 2.7.STABLE3 - **committed - 12931**
  * 12230 - HEAD - Plug some "squid -k reconfigure" memory leaks. Mostly SSL related. - **committed - 12932**
  * 12231 - HEAD - [Bug #1993](https://code.google.com/p/lusca-cache/issues/detail?id=1993): Memory leak in http\_reply\_access deny processing - **committed - 12934**
  * 12232 - HEAD - [Bug #2378](https://code.google.com/p/lusca-cache/issues/detail?id=2378): Duplicate paths in FwdServers - **committed - 12933**
  * 12233 - HEAD - Report the cache\_peer name instead of hostname - **committed - 12935**

  * 12234 - HEAD - [Bug #2388](https://code.google.com/p/lusca-cache/issues/detail?id=2388): acl documentation cleanup - **committed - 12941**
  * 12235 - HEAD - [Bug #2122](https://code.google.com/p/lusca-cache/issues/detail?id=2122): In some situations collapsed\_forwarding could leak private information - **committed - 12942**
  * 12236 - HEAD - [Bug #2365](https://code.google.com/p/lusca-cache/issues/detail?id=2365): cachemgr.cgi fails to HTML encode config dumps properly - **committed - 12943**
  * 12245 - HEAD - [Bug #2376](https://code.google.com/p/lusca-cache/issues/detail?id=2376): Round-Robin becomes unbalanced when a peer dies and comes back - **committed - 12944**
  * 12247 - HEAD - Documentation update preparing for 2.7.STABLE3 - **committed - 12945**
  * 12251 - HEAD - [Bug #2392](https://code.google.com/p/lusca-cache/issues/detail?id=2392): Oversized chunk header on port .. regression error - **committed 12946**
  * 12253 - HEAD - Extend mgr:idns output to include the actual queries and the protocol used. - **committed - 12947**
  * 12254 - HEAD - [Bug #2387](https://code.google.com/p/lusca-cache/issues/detail?id=2387): The calculation of the number of hash buckets need to account for the memory size, not only disk size - **committed - 12948**
  * 12255 - HEAD - [Bug #2390](https://code.google.com/p/lusca-cache/issues/detail?id=2390): New hier\_code ACL type - **committed - 12949**

  * 12257 - HEAD - [Bug #2393](https://code.google.com/p/lusca-cache/issues/detail?id=2393): DNS requests retried indefinitely at full speed on failed TCP connection - **committed - 12959**
  * 12258 - HEAD - [Bug #2393](https://code.google.com/p/lusca-cache/issues/detail?id=2393): DNS retransmit queue could get hold up - **committed - 12960**
  * 12259 - HEAD - Correct socket syscalls statistics in commResetFD() - **committed - 12961**
  * 12279 - HEAD - Preparign for 2.6.STABLE20 - **committed - 12962**
  * 12283 - HEAD - Plug a small "squid -k reconfigure" race in the new round-robin counter management - **committed - 12963**

  * 12292 - HEAD - Windows port: fix typo in handling of notification of IP address changes - **committed - 13041**
  * 12295 - HEAD - Document the "zph\_mode option" setting, got left out when merging the zph patches - **committed - 13042**
  * 12296 - HEAD - [Bug 2396](https://code.google.com/p/lusca-cache/issues/detail?id=396): Correct the opening of the PF device file. - **committed - 13043**
  * 12297 - HEAD - URL encode the user name sent to digest helpers to escape "odd" characters such as doublequote (") - **committed - 13044**
  * 12298 - HEAD - auth\_param basic&digest utf8 on|of - **committed - 13045**
  * 12299 - HEAD - Back out the digest url escaping change. - **committed - 13047**
  * 12300 - HEAD - Make --with-large-files and --with-build-envirnment=default play nice together - **committed - 13048**
  * 12301 - HEAD - Bootstrapped - **committed - 13049**
  * 12302 - HEAD - [Bug #2407](https://code.google.com/p/lusca-cache/issues/detail?id=2407): Spelling error in http\_port tcpkeepalive option - **committed - 13050**
  * 12303 - HEAD - [Bug #2408](https://code.google.com/p/lusca-cache/issues/detail?id=2408): assertion failed: forward.c:529: "fs" - **committed - 13051**
  * 12304 - HEAD - [Bug #2408](https://code.google.com/p/lusca-cache/issues/detail?id=2408): Make forwarding logic a little more robust - **committed - 13052**
  * 12311 - HEAD - Remove the --disable-carp option, keeping the CARP code always compiled in - **committed - 13053**
  * 12312 - HEAD - Update userhash and sourcehash implementations - **committed - 13055**
  * 12313 - HEAD - copy-paste spelling error in the updated userhash and sourcehash implementations - **commtited - 13056**
  * 12314 - HEAD - Workaround for Linux-2.6.24 & 2.6.25 netfiler\_ipv4.h include header u32 problem - **committed - 13057**
  * 12315 - HEAD - Correct sourcehash/userhash cachemgr descriptions - **committed - 13058**
  * 12316 - HEAD - Make dns\_nameserver work when using --disable-internal-dns on glibc based systems - **committed - 13059**
  * 12317 - HEAD - Bootstrapped - **committed - 13060**
  * 12318 - HEAD - [Bug #2414](https://code.google.com/p/lusca-cache/issues/detail?id=2414): assertion failed: forward.c:110: "!EBIT\_TEST(e->flags, ENTRY\_FWD\_HDR\_WAIT)" - **committed - 13061**
  * 12323 - HEAD - Make clientCacheHit bail out gracefuly if hitting an aborted object - **committed - 13062**
  * 12325 - HEAD - Handle digest fetch errors gracefully - **committed - 13063**
  * 12326 - HEAD - Allow reading of aborted objects - **committed - 13064**
  * 12327 - HEAD - make clientHandleIMSReply deal properly if the old object got aborted - **committed - 13065**
  * 12328 - HEAD - [Bug #2406](https://code.google.com/p/lusca-cache/issues/detail?id=2406): access.log logs rewritten URL and strip\_query\_terms ineffective - **committed - 13066**
  * 12330 - HEAD - Properly terminate aborted objects. Got broken again by the change to loosen ENTRY\_ABORTED semantics. - **committed - 13067**

  * 12337 - HEAD - Increase buffer in authenticateNegotiateStart - **committed - 13603**
  * 12338 - HEAD - Help output with squid\_ldap\_group - **committed - 13604**

  * 12341 - HEAD - Prepare for 2.7.STABLE4 - **committed - 13606**
  * 12352 - HEAD - upgrade\_http0.9 option - **committed - 13610**
  * 12353 - HEAD - Benno's work on supporting arbitrary unknown methods - **committed - 13611**
  * 12354 - HEAD - Trim trailing whitespace - **committed - 13612**
  * 12355 - HEAD - Make purge behaviour more RFC compliant - **committed - 13613**
  * 12356 - HEAD - UTF-8 in config file where it shouldn't be - **committed - 13614**
  * 12357 - HEAD - Fix digest code w/ method changes - **committed - 13615**
  * 12358 - HEAD - GCC warnings (**NOTE**: this is potentially dangerous code?) - **committed - 13616**
  * 12359 - HEAD - MIME type ACL clarification - **committed - 13617**
  * 12360 - HEAD - convert upgrade\_http0.9 to ACL; add header - **committed - 13618**
  * 12361 - HEAD - Generate squid.conf.clean - **committed - 13619**
  * 12362 - HEAD - Update http0.9 default docs - **committed - 13620**
  * 12364 - HEAD - Fix GCC related breakage - **committed - 13622**
  * 12365 - HEAD - bootstrapped - **committed - 13623**
  * 12366 - HEAD - HTTP CLR enhancements - **committed - 13624**
  * 12367 - HEAD - Remove unneeded checks - **committed - 13625**
  * 12368 - HEAD - Sync urlAbsolute / httpRemovePublicByHeader with Squid3 - **committed - 13626**
  * 12369 - HEAD - Fix STABLE\_BRANCH - **committed - 13627**
  * 12370 - HEAD - Langpack compatibility stuff - **committed - 13628**
  * 12371 - HEAD - Fix rebuild logic a bit - **committed - 13629**
  * 12372 - HEAD - bootstrapped - **committed - 13630**
  * 12373 - HEAD - Fix aborted IMS object handling - **committed - 13631**
  * 12379 - HEAD - Increase kerberos token buffer length - **committed - 13632**
  * 12381 - HEAD - Limit stale-if-error to 500-504 responses - **committed - 13633**
  * 12384 - HEAD - Really add new templates - **committed - 13634**
  * 12385 - HEAD - fix perm issues with -k reconfigure - **committed - 13635**
  * 12386 - HEAD - Return new headers after IMS request validation - **committed - 13636**
  * 12388 - HEAD - Fix build error on windows - **committed - 13637**
  * 12397 - HEAD - Don't set expire header on error pages - **committed - 13638**
  * 12398 - HEAD - Windows port fixes - **committed - 13639**
  * 12401 - HEAD - back out purge method changes; fixes IMS handling - **committed - 13640**
  * 12402 - HEAD - Delayed forwarding support - **committed - 13641**
  * 12404 - HEAD - Prepare 2.7.STABLE5 - **committed - 13642**
  * 12407 - HEAD - Prepare 2.6.STABLE22 - **committed - 13643**
  * 12410 - HEAD - Fix tproxy url - **committed - 13644**
  * 12411 - HEAD - Bootstrapped - **committed - 13645**
  * 12416 - HEAD - 2.6.STABLE22 ChangeLog - **committed - 13646**
  * 12417 - HEAD - Correct Latency measurements - **committed - 13647**
  * 12418 - HEAD - Fix kqueue typo in above patch - **committed - 13648**
  * 12419 - HEAD - collapsed\_forwarding\_timeout directive - **committed - 13649**
  * 12420 - HEAD - Fix error in upgrade\_http0.9 rule - **committed - 13650**
  * 12421 - HEAD - Bootstrapped - **committed - 13651**
  * 12423 - HEAD - Cross-reference authenticate\_ip\_shortcircuit\_access and authenticate\_ip\_shortcircuit\_ttl - **committed - 13652**
  * 12424 - HEAD - Fix Visual Studio errors - **committed - 13653**

## Stuff merged into trunk but not yet into CACHEBOY\_HEAD ##

  * 12339 - HEAD - Adrian's write-side delay pool code - **committed - 13605**
  * 12346 - HEAD - conditional compile delay pools code - **committed - 13607**
  * 12347 - HEAD - Documentation update for 2.7.STABLE4 - **committed - 13608**
  * 12351 - HEAD - Namespace cleanup after the delay\_access fix - **committed - 13609**

## Stuff merged into trunk but not into CACHEBOY\_HEAD because something else already exists implementing that patch ##

  * 12363 - HEAD - Shutdown store\_url helpers on reconfigure - **committed - 13621**

# Introduction #

The Squid store rebuild and store logging code doesn't scale. It uses sync disk IO calls to read and write swaplogs; it makes explicit assumptions about the state of the system when writing out the clean swaplog (ie, that the clean swaplog event is the only task running.) All of these problems make it scale poorly under load and with a lot of disk storage.

# Project Aims #

As nice as it would be to break this out into multiple threads doing "stuff", the codebase is still not really ready for any further worker thread code just yet. One of the aims is to have this work leave the codebase in a sufficiently nice(r) state to make migrating to a worker thread model easier in the future.

  * Make "rotate" be quick and simple. Don't have it write out clean swaplogs.
  * Make the logfile writing and reading be async (ie, non blocking) rather than the current blocking IO methods used.
  * Think about how to possibly "fix" COSS rebuilding by combining both this new async logfile writing and a "rebuild helper" which can (ab)use written out logs for rebuild instead of the current highly dirty method.

The eventual goal is to dramatically improve the startup, shutdown, rotate and general runtime performance of the store logging code.

In the AUFS case, parallelising the rebuilds and using async IO for reading large swaplogs will make the restart process take seconds or minutes instead of potentially hours/days on a large cache. Rebuilding from disk will occur completely transparently in the background without affecting the main process - lusca can then start immediately returning "HITs" where possible.

In the COSS case, keeping a cyclic-type swaplog will make it much, much easier to do a rebuild. Instead of reading the entire disk looking for objects, the swaplog can be used to rebuild the index. Future work should investigate an alternative COSS disk layout which doesn't require reading the entire disk to recover objects.

# Specific Bits #

Implement a reliable and asynchronous clean "swaplog" writing method; even if that is a separate command. This way "rotate" can occur once an hour, but "write clean logs" can occur daily.

Modify the AUFS swaplog writing code to use asynchronous IO somehow. This can either be using the existing aiops code, or a "writer helper" (akin to the logfile daemon code at the moment), or maybe a helper thread if there's sufficient motivation.

Modify the AUFS swaplog reading code to use an external helper. External helper programs already exist which implement "cat swaplog" and "generate a swaplog from the directory contents".

# Code Projects #

  * /playpen/LUSCA\_HEAD\_storework - implement the store rebuild helper framework; migrate AUFS/COSS to use it - **done**
  * /playpen/LUSCA\_HEAD\_store\_clean\_log\_work - look at the method used to write out the clean swap logs (for squid -k rotate, squid -k shutdown, etc) and document/tidy it up in preparation for asynchronous swaplog writing


# Completed tasks #

  * The helper process has been completed - it generates a stream of swaplog entries from either a logfile or the directory itself.
  * The AUFS rebuild process now uses the helper to rebuild the storedir from either the log or the directory
  * The COSS rebuild process now uses another helper to rebuild the storedir from the directory. Log rebuilding will come later.

# Weirdnesses to address #

  * If the ipcCreate() calls fails the storedir is not rebuilt and Lusca continues with a blank storedir. The trouble is it writes out a single-entry swap.state file which destroys the valid logfile which may be there. This should be twiddled as soon as possible. (Possibly make sure ipcCreate() succeeds before calling the tmp swap log open and close functions?) - **fixed**

# Time Estimates #

  * Initial planning - mostly done, about a week's worth
  * Development - AUFS - two weeks
  * Development - COSS - one week
  * (Thorough!) Testing - two weeks
  * Documentation - one week
  * Live deployment - depends entirely on how many users deploy it and provide feedback!

# Future work #

One idea I've had is to change the "rebuild from disk" logic into two parts - one to populate the file number bitmap, and then one to rebuild the item indexes. This way the proxy can immediately start serving hits -and- storing new items without overwriting existing items in the cache.
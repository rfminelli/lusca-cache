# Ongoing Development #

  * /playpen/LUSCA\_HEAD\_module - code modularisation work (shared object support)
  * /playpen/LUSCA\_HEAD\_strref - reference counted NUL terminated C strings (cheap string dup())
  * /playpen/LUSCA\_HEAD\_http\_vector - vectorize the HttpHeaderEntry allocation to dramatically reduce malloc/free calls
  * /playpen/LUSCA\_HEAD\_zerocopy\_storeread - [ProjectAsyncReadCopy](ProjectAsyncReadCopy.md) - eliminate the memcpy() in the async IO read path; begin evaluating further asynchronous event improvements
  * /playpen/LUSCA\_HEAD\_store\_clean\_log\_rework - [ProjectStoreRebuildChanges](ProjectStoreRebuildChanges.md) - work on the clean store log writing process

# Stale Development #

(TODO)

# Completed Development #

  * /playpen/LUSCA\_HEAD\_errorpages - pretty error page template generation framework stuff
  * /playpen/LUSCA\_HEAD\_storework - [ProjectStoreRebuildChanges](ProjectStoreRebuildChanges.md) - introduce store rebuild helpers; tidy up rebuild logic
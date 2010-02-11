#ifndef	__CLIENT_SIDE_H__
#define	__CLIENT_SIDE_H__

/*
 * These routines are exported to other client-side modules but not
 * to the rest of the codebase. They form a transition API whilst
 * the codebase is being further reorganised and refactored.
 * Do not write new code which calls them unless you're absolutely
 * sure of what you're doing!
 */
extern void clientProcessHit(clientHttpRequest * http);
extern void clientProcessMiss(clientHttpRequest * http);
extern void clientProcessRequest(clientHttpRequest *);
extern void clientProcessExpired(clientHttpRequest *);
extern void clientProcessOnlyIfCachedMiss(clientHttpRequest * http);
extern void httpRequestFree(void *data);


#endif

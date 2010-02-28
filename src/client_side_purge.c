
#include "squid.h"

#include "client_side.h"
#include "client_side_purge.h"


void
clientPurgeRequest(clientHttpRequest * http)
{
    StoreEntry *entry;
    ErrorState *err = NULL;
    HttpReply *r;
    http_status status = HTTP_NOT_FOUND;
    method_t *method_get = NULL, *method_head = NULL;
    debug(33, 3) ("Config2.onoff.enable_purge = %d\n", Config2.onoff.enable_purge);
    if (!Config2.onoff.enable_purge) {
	http->log_type = LOG_TCP_DENIED;
	err = errorCon(ERR_ACCESS_DENIED, HTTP_FORBIDDEN, http->orig_request);
	http->entry = clientCreateStoreEntry(http, http->request->method, null_request_flags);
	errorAppendEntry(http->entry, err);
	return;
    }
    /* Release both IP cache */
    ipcacheInvalidate(http->request->host);

    method_get = urlMethodGetKnownByCode(METHOD_GET);
    method_head = urlMethodGetKnownByCode(METHOD_HEAD);

    if (!http->flags.purging) {
	/* Try to find a base entry */
	http->flags.purging = 1;
	entry = storeGetPublicByRequestMethod(http->request, method_get);
	if (!entry) {
	    entry = storeGetPublicByRequestMethod(http->request, method_head);
	}
	if (entry) {
	    if (EBIT_TEST(entry->flags, ENTRY_SPECIAL)) {
		http->log_type = LOG_TCP_DENIED;
		err = errorCon(ERR_ACCESS_DENIED, HTTP_FORBIDDEN, http->request);
		http->entry = clientCreateStoreEntry(http, http->request->method, null_request_flags);
		errorAppendEntry(http->entry, err);
		return;
	    }
	    /* Swap in the metadata */
	    http->entry = entry;
	    storeLockObject(http->entry);
	    storeCreateMemObject(http->entry, http->uri);
	    http->entry->mem_obj->method = http->request->method;
	    http->sc = storeClientRegister(http->entry, http);
	    http->log_type = LOG_TCP_HIT;
	    storeClientCopyHeaders(http->sc, http->entry,
		clientCacheHit,
		http);
	    return;
	}
    }
    http->log_type = LOG_TCP_MISS;
    /* Release the cached URI */
    entry = storeGetPublicByRequestMethod(http->request, method_get);
    if (entry) {
	debug(33, 4) ("clientPurgeRequest: GET '%s'\n",
	    storeUrl(entry));
#if USE_HTCP
	neighborsHtcpClear(entry, NULL, http->request, method_get, HTCP_CLR_PURGE);
#endif
	storeRelease(entry);
	status = HTTP_OK;
    }
    entry = storeGetPublicByRequestMethod(http->request, method_head);
    if (entry) {
	debug(33, 4) ("clientPurgeRequest: HEAD '%s'\n",
	    storeUrl(entry));
#if USE_HTCP
	neighborsHtcpClear(entry, NULL, http->request, method_head, HTCP_CLR_PURGE);
#endif
	storeRelease(entry);
	status = HTTP_OK;
    }
    /* And for Vary, release the base URI if none of the headers was included in the request */
    if (http->request->vary_headers && !strstr(http->request->vary_headers, "=")) {
	entry = storeGetPublic(urlCanonical(http->request), method_get);
	if (entry) {
	    debug(33, 4) ("clientPurgeRequest: Vary GET '%s'\n",
		storeUrl(entry));
#if USE_HTCP
	    neighborsHtcpClear(entry, NULL, http->request, method_get, HTCP_CLR_PURGE);
#endif
	    storeRelease(entry);
	    status = HTTP_OK;
	}
	entry = storeGetPublic(urlCanonical(http->request), method_head);
	if (entry) {
	    debug(33, 4) ("clientPurgeRequest: Vary HEAD '%s'\n",
		storeUrl(entry));
#if USE_HTCP
	    neighborsHtcpClear(entry, NULL, http->request, method_head, HTCP_CLR_PURGE);
#endif
	    storeRelease(entry);
	    status = HTTP_OK;
	}
    }
    /*
     * Make a new entry to hold the reply to be written
     * to the client.
     */
    http->entry = clientCreateStoreEntry(http, http->request->method, null_request_flags);
    httpReplyReset(r = http->entry->mem_obj->reply);
    httpReplySetHeaders(r, status, NULL, NULL, 0, -1, squid_curtime);
    httpReplySwapOut(r, http->entry);
    storeComplete(http->entry);
}

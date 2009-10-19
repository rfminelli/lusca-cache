#include "include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "include/util.h"
#include "include/Array.h"
#include "include/Stack.h"

#include "libcore/kb.h"
#include "libcore/gb.h"
#include "libcore/dlink.h"
#include "libcore/varargs.h"
#include "libcore/tools.h"

#include "libmem/MemPool.h"
#include "libmem/MemBufs.h"
#include "libmem/MemBuf.h"

#include "libsqinet/sqinet.h"

#include "libsqdebug/debug.h"

#include "libhelper/ipc.h"

#include "libiapp/fd_types.h"
#include "libiapp/comm_types.h"
#include "libiapp/comm.h"

#include "loghelper.h"
#include "loghelper_commands.h"


static void
loghelper_buffer_free(loghelper_buffer_t *lb)
{
	safe_free(lb->buf);
	safe_free(lb);
}

loghelper_buffer_t *
loghelper_buffer_create(void)
{
	loghelper_buffer_t *lb;

	lb = xcalloc(1, sizeof(*lb));

	lb->buf = xmalloc(32768);
	lb->size = 32768;

	return lb;
}

/* *** */

/*
 * Free all pending buffers
 */
static void
loghelper_free_buffers(loghelper_instance_t *lh)
{
	loghelper_buffer_t *lb;

	while (lh->bufs.head == NULL) {
		lb = lh->bufs.head->data;
		dlinkDelete(&lb->node, &lh->bufs);
		loghelper_buffer_free(lb);
	}
	lh->nbufs = 0;
}

/*
 * destroy the loghelper.
 *
 * Destroy any/all pending buffers, deregister/close the sockets, terminate the helper.
 * Then free the helper.
 */
static void
loghelper_destroy(loghelper_instance_t *lh)
{
	/* free buffers */
	loghelper_free_buffers(lh);

	/* close comm sockets - which will hopefully gracefully shut down the helper later */
	comm_close(lh->rfd);
	if (lh->rfd != lh->wfd)
		comm_close(lh->wfd);

	/* Free the loghelper instance */
	safe_free(lh);
}

static int
loghelper_has_bufs(loghelper_instance_t *lh)
{
	return (lh->nbufs > 0);
}

/*
 * Create a loghelper instance.
 *
 * This creates the instance state, forks off the logfile helper and sets everything up to begin
 * accepting queued data.
 */
loghelper_instance_t *
loghelper_create(const char *path, const char *progname, const char *args[])
{
	loghelper_instance_t * lh;

	lh = xcalloc(1, sizeof(*lh));

	lh->pid = ipcCreate(IPC_STREAM, path, args, progname, 0, &lh->rfd, &lh->wfd, NULL);
	if (lh->pid < 0) {
		debug(87, 1) ("loghelper_create: %s: couldn't create program: %s\n", path, xstrerror());
		safe_free(lh);
		return NULL;
	}
	return lh;
}

/*
 * Close down a loghelper instance.
 *
 * This queues a close and quit message, then marks the loghelper as "going away."
 *
 * The loghelper is not freed here - it will wait for some response (or error) from
 * the loghelper process before finalising the current state.
 *
 * Any pending messages will be flushed and sent out before the close is queued.
 *
 * The "lh" pointer should be viewed as invalid after this call.
 */
void
loghelper_close(loghelper_instance_t *lh)
{
	/* Mark the connection as closing */
	lh->flags.closing = 1;

	/* Are there pending messages? If so, leave it for now and queue a write if needed */
	if (loghelper_has_bufs(lh)) {
		/* XXX check if an explicit flush needs to be scheduled here */
		return;
	}

	/* No pending messages? Wrap up. */
	loghelper_destroy(lh);
}

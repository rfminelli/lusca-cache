#include "include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "include/util.h"

#include "libcore/kb.h"
#include "libcore/dlink.h"
#include "libcore/varargs.h"
#include "libcore/tools.h"

#include "libsqdebug/debug.h"

#include "libhelper/ipc.h"

#include "loghelper.h"
#include "loghelper_commands.h"

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

}

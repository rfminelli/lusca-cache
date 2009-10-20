#include "include/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
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


/* Loghelper buffer management functions */

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

/* ********** */
/* Loghelper instance buffer management functions */

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

static void
loghelper_append_buffer(loghelper_instance_t *lh, loghelper_buffer_t *lb)
{
	dlinkAddTail(lb, &lb->node, &lh->bufs);
	lh->nbufs++;
}

static int
loghelper_has_bufs(loghelper_instance_t *lh)
{
	return (lh->nbufs > 0);
}

static loghelper_buffer_t *
loghelper_dequeue_buffer(loghelper_instance_t *lh)
{
	loghelper_buffer_t *lb;

	if (lh->bufs.head == NULL)
		return NULL;
	lb = lh->bufs.head->data;
	dlinkDelete(&lb->node, &lh->bufs);
	lh->nbufs--;
	assert(lh->nbufs >= 0);
	return lb;
}

static void
loghelper_requeue_buffer(loghelper_instance_t *lh, loghelper_buffer_t *lb)
{
	dlinkAdd(lb, &lb->node, &lh->bufs);
	lh->nbufs++;
}


/* ********* */
/* Loghelper instance related functions */

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

static void
loghelper_write_handle(int fd, void *data)
{
	loghelper_instance_t *lh = data;
	loghelper_buffer_t *lb;
	int r;

	debug(87, 5) ("loghelper_write_handle: %p: write ready; writing\n", lh);
	lh->flags.writing = 0;

	/* Dequeue a buffer */
	lb = loghelper_dequeue_buffer(lh);

	/* Write it to the socket */
	r = FD_WRITE_METHOD(fd, lb->buf + lb->written_len, lb->len - lb->written_len);

	/* EOF? Error? Shut down the IPC sockets for now and flag it */
	/* And requeue the dequeued buffer so it is eventually retried when appropriate */
	assert(r > 0);

	/* Was it partially written? Requeue the buffer for another write */
	if (r < lb->len) {
		lb->written_len += r;
		loghelper_requeue_buffer(lh, lb);
	} else {
		loghelper_buffer_free(lb);
	}

	/* Is there any further data? schedule another write */
	if (loghelper_has_bufs(lh)) {
		lh->flags.writing = 1;
		commSetSelect(lh->wfd, COMM_SELECT_WRITE, loghelper_write_handle, lh, 0);
	}

	/* Out of buffers and closing? Destroy the instance */
	if (lh->flags.closing && (! loghelper_has_bufs(lh))) {
		loghelper_destroy(lh);
		return;
	}
}

static void
loghelper_kick_write(loghelper_instance_t *lh)
{
	/* Don't queue a write if we already have */
	if (lh->flags.writing)
		return;

	/* Queue write and set flag to make sure we don't queue another */
	commSetSelect(lh->wfd, COMM_SELECT_WRITE, loghelper_write_handle, lh, 0);
	lh->flags.writing = 1;
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
		loghelper_kick_write(lh);
		return;
	}

	/* No pending messages? Wrap up. */
	loghelper_destroy(lh);
}

/*
 * Queue a command to be sent to the loghelper.
 *
 * The command will be queued to be sent straight away. Subsequently queued requests
 * will wait until the socket write buffer is ready for more data.
 *
 * For now there's an artificial 32768 - 3 byte limit on the payload size.
 */
int
loghelper_queue_command(loghelper_instance_t *lh, loghelper_command_t cmd, short payload_len, const char payload[])
{
	u_int8_t u_cmd;
	u_int16_t u_len;
	loghelper_buffer_t *lb;

	/* make sure the data will fit for now */
	if (payload_len + 3 > 32768)
		return 0;

	u_cmd = cmd;
	u_len = payload_len + 3;		/* 3 == 1 byte cmd, 2 byte length */

	/* Create a new buffer */
	lb = loghelper_buffer_create();

	/* Copy in the command */
	lb->buf[0] = 0;
	lb->buf[1] = u_cmd;

	/* Copy in the packet length */
	lb->buf[1] = (u_len & 0xff00) >> 8;	/* encode high byte */
	lb->buf[2] = (u_len & 0xff);		/* .. and low byte, giving us network byte order */

	/* Copy in the payload, if any */
	if (payload_len > 0)
		memcpy(lb->buf + 4, payload, payload_len);

	/* Add it to the instance buffer list tail */
	loghelper_append_buffer(lh, lb);

	/* Do we have a write scheduled? If not, schedule a write */
	loghelper_kick_write(lh);

	return 1;
}

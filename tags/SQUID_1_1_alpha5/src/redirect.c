/*
 * $Id$
 *
 * DEBUG: section 29    Redirector
 * AUTHOR: Duane Wessels
 *
 * SQUID Internet Object Cache  http://www.nlanr.net/Squid/
 * --------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from the
 *  Internet community.  Development is led by Duane Wessels of the
 *  National Laboratory for Applied Network Research and funded by
 *  the National Science Foundation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  
 */

#include "squid.h"

#define REDIRECT_FLAG_ALIVE		0x01
#define REDIRECT_FLAG_BUSY		0x02
#define REDIRECT_FLAG_CLOSING	0x04

typedef struct {
    int fd;
    void *data;
    char *orig_url;
    RH handler;
} redirectStateData;

typedef struct _redirector {
    int index;
    int flags;
    int fd;
    char *inbuf;
    unsigned int size;
    unsigned int offset;
    struct timeval dispatch_time;
    redirectStateData *redirectState;
} redirector_t;

static struct {
    int requests;
    int replies;
    int errors;
    int avg_svc_time;
    int queue_size;
    int use_hist[DefaultRedirectChildrenMax];
} RedirectStats;


struct redirectQueueData {
    struct redirectQueueData *next;
    redirectStateData *redirectState;
};

/* GLOBALS */
int do_redirect = 0;

static redirector_t *GetFirstAvailable _PARAMS((void));
static int redirectCreateRedirector _PARAMS((char *command));
static int redirectHandleRead _PARAMS((int, redirector_t *));
static redirectStateData *Dequeue _PARAMS(());
static void Enqueue _PARAMS((redirectStateData *));
static void redirectDispatch _PARAMS((redirector_t *, redirectStateData *));


static redirector_t **redirect_child_table = NULL;
static int NRedirectors = 0;
static int NRedirectorsOpen = 0;
static struct redirectQueueData *redirectQueueHead = NULL;
static struct redirectQueueData **redirectQueueTailP = &redirectQueueHead;

static int redirectCreateRedirector(command)
     char *command;
{
    int pid;
    u_short port;
    struct sockaddr_in S;
    static int n_redirector = 0;
    int cfd;
    int sfd;
    int len;
    int fd;
    cfd = comm_open(COMM_NOCLOEXEC,
	local_addr,
	0,
	"socket to redirector");
    if (cfd == COMM_ERROR) {
	debug(29, 0, "redirect_create_redirector: Failed to create redirector\n");
	return -1;
    }
    len = sizeof(S);
    memset(&S, '\0', len);
    if (getsockname(cfd, (struct sockaddr *) &S, &len) < 0) {
	debug(29, 0, "redirect_create_redirector: getsockname: %s\n", xstrerror());
	comm_close(cfd);
	return -1;
    }
    port = ntohs(S.sin_port);
    listen(cfd, 1);
    if ((pid = fork()) < 0) {
	debug(29, 0, "redirect_create_redirector: fork: %s\n", xstrerror());
	comm_close(cfd);
	return -1;
    }
    if (pid > 0) {		/* parent */
	comm_close(cfd);	/* close shared socket with child */
	/* open new socket for parent process */
	sfd = comm_open(0, local_addr, 0, NULL);	/* blocking! */
	if (sfd == COMM_ERROR)
	    return -1;
	if (comm_connect(sfd, localhost, port) == COMM_ERROR) {
	    comm_close(sfd);
	    return -1;
	}
	comm_set_fd_lifetime(sfd, -1);
	debug(29, 4, "redirect_create_redirector: FD %d connected to %s #%d.\n",
	    sfd, command, n_redirector++);
	return sfd;
    }
    /* child */
    no_suid();			/* give up extra priviliges */
    if ((fd = accept(cfd, NULL, NULL)) < 0) {
	debug(29, 0, "redirect_create_redirector: FD %d accept: %s\n",
	    cfd, xstrerror());
	_exit(1);
    }
    dup2(fd, 0);
    dup2(fd, 1);
    for (fd = 3; fd < FD_SETSIZE; fd++)
	close(fd);
    execlp(command, "(redirector)", NULL);
    debug(29, 0, "redirect_create_redirector: %s: %s\n", command, xstrerror());
    _exit(1);
    return 0;
}

static int redirectHandleRead(fd, redirector)
     int fd;
     redirector_t *redirector;
{
    int len;
    redirectStateData *r = redirector->redirectState;
    char *t = NULL;
    int n;
    int svc_time;

    len = read(fd,
	redirector->inbuf + redirector->offset,
	redirector->size - redirector->offset);
    debug(29, 5, "redirectHandleRead: %d bytes from Redirector #%d.\n",
	len, redirector->index + 1);
    if (len <= 0) {
	if (len < 0)
	    debug(29, 1, "redirectHandleRead: FD %d read: %s\n", fd, xstrerror());
	debug(29, redirector->flags & REDIRECT_FLAG_CLOSING ? 5 : 1,
	    "FD %d: Connection from Redirector #%d is closed, disabling\n",
	    fd, redirector->index + 1);
	redirector->flags = 0;
	comm_close(fd);
	if (--NRedirectorsOpen == 0)
	    fatal_dump("All redirectors have exited!");
	return 0;
    }
    redirector->offset += len;
    redirector->inbuf[redirector->offset] = '\0';
    /* reschedule */
    comm_set_select_handler(redirector->fd,
	COMM_SELECT_READ,
	(PF) redirectHandleRead,
	redirector);
    if ((t = strchr(redirector->inbuf, '\n'))) {
	/* end of record found */
	*t = '\0';
	debug(29, 5, "redirectHandleRead: reply: '%s'\n", redirector->inbuf);
	if (r->handler) {
	    r->handler(r->data,
		t == redirector->inbuf ? NULL : redirector->inbuf);
	}
	safe_free(r);
	redirector->redirectState = NULL;
	redirector->flags &= ~REDIRECT_FLAG_BUSY;
	redirector->offset = 0;
	n = ++RedirectStats.replies;
	svc_time = tvSubMsec(redirector->dispatch_time, current_time);
	if (n > REDIRECT_AV_FACTOR)
	    n = REDIRECT_AV_FACTOR;
	RedirectStats.avg_svc_time
	    = (RedirectStats.avg_svc_time * (n - 1) + svc_time) / n;
    }
    while ((redirector = GetFirstAvailable()) && (r = Dequeue()))
	redirectDispatch(redirector, r);
    return 0;
}

static void Enqueue(r)
     redirectStateData *r;
{
    struct redirectQueueData *new = xcalloc(1, sizeof(struct redirectQueueData));
    new->redirectState = r;
    *redirectQueueTailP = new;
    redirectQueueTailP = &new->next;
    RedirectStats.queue_size++;
}

static redirectStateData *Dequeue()
{
    struct redirectQueueData *old = NULL;
    redirectStateData *r = NULL;
    if (redirectQueueHead) {
	r = redirectQueueHead->redirectState;
	old = redirectQueueHead;
	redirectQueueHead = redirectQueueHead->next;
	if (redirectQueueHead == NULL)
	    redirectQueueTailP = &redirectQueueHead;
	safe_free(old);
	RedirectStats.queue_size--;
    }
    return r;
}

static redirector_t *GetFirstAvailable()
{
    int k;
    redirector_t *redirect = NULL;
    for (k = 0; k < NRedirectors; k++) {
	redirect = *(redirect_child_table + k);
	if (!(redirect->flags & REDIRECT_FLAG_BUSY))
	    return redirect;
    }
    return NULL;
}


static void redirectDispatch(redirect, r)
     redirector_t *redirect;
     redirectStateData *r;
{
    char *buf = NULL;
    int len;
    if (r->handler == NULL) {
	debug(29, 1, "redirectDispatch: skipping '%s' because no handler\n",
	    r->orig_url);
	safe_free(r);
	return;
    }
    redirect->flags |= REDIRECT_FLAG_BUSY;
    redirect->redirectState = r;
    redirect->dispatch_time = current_time;
    len = strlen(r->orig_url) + 1;
    buf = xmalloc(len + 1);
    sprintf(buf, "%s\n", r->orig_url);
    comm_write(redirect->fd,
	buf,
	len,
	0,			/* timeout */
	NULL,			/* Handler */
	NULL);			/* Handler-data */
    debug(29, 5, "redirectDispatch: Request sent to Redirector #%d, %d bytes\n",
	redirect->index + 1, len);
    RedirectStats.use_hist[redirect->index]++;
    RedirectStats.requests++;
}


/**** PUBLIC FUNCTIONS ****/


void redirectStart(url, fd, handler, data)
     char *url;
     int fd;
     RH handler;
     void *data;
{
    redirectStateData *r = NULL;
    redirector_t *redirector = NULL;
    if (!handler)
	fatal_dump("redirectStart: NULL handler");
    if (!do_redirect) {
	(*handler) (data, NULL);
	return;
    }
    r = xcalloc(1, sizeof(redirectStateData));
    r->fd = fd;
    r->orig_url = url;
    r->handler = handler;
    r->data = data;
    if ((redirector = GetFirstAvailable()))
	redirectDispatch(redirector, r);
    else
	Enqueue(r);
}

void redirectOpenServers()
{
    char *prg = getRedirectProgram();
    int k;
    int redirectsocket;
    static char fd_note_buf[FD_ASCII_NOTE_SZ];
    static int first_time = 0;

    if (!do_redirect)
	return;
    /* free old structures if present */
    if (redirect_child_table) {
	for (k = 0; k < NRedirectors; k++)
	    safe_free(redirect_child_table[k]);
	safe_free(redirect_child_table);
    }
    NRedirectors = NRedirectorsOpen = getRedirectChildren();
    redirect_child_table = xcalloc(NRedirectors, sizeof(redirector_t *));
    debug(29, 1, "redirectOpenServers: Starting %d '%s' processes\n",
	NRedirectors, prg);
    for (k = 0; k < NRedirectors; k++) {
	redirect_child_table[k] = xcalloc(1, sizeof(redirector_t));
	if ((redirectsocket = redirectCreateRedirector(prg)) < 0) {
	    debug(29, 1, "WARNING: Cannot run '%s' process.\n", prg);
	    redirect_child_table[k]->flags &= ~REDIRECT_FLAG_ALIVE;
	} else {
	    redirect_child_table[k]->flags |= REDIRECT_FLAG_ALIVE;
	    redirect_child_table[k]->index = k;
	    redirect_child_table[k]->fd = redirectsocket;
	    redirect_child_table[k]->inbuf = get_free_4k_page();
	    redirect_child_table[k]->size = 4096;
	    redirect_child_table[k]->offset = 0;
	    sprintf(fd_note_buf, "%s #%d",
		prg,
		redirect_child_table[k]->index + 1);
	    fd_note(redirect_child_table[k]->fd, fd_note_buf);
	    commSetNonBlocking(redirect_child_table[k]->fd);
	    /* set handler for incoming result */
	    comm_set_select_handler(redirect_child_table[k]->fd,
		COMM_SELECT_READ,
		(PF) redirectHandleRead,
		(void *) redirect_child_table[k]);
	    debug(29, 3, "redirectOpenServers: 'redirect_server' %d started\n",
		k);
	}
    }
    if (first_time == 0) {
	first_time++;
	memset(&RedirectStats, '\0', sizeof(RedirectStats));
    }
}

void redirectShutdownServers()
{
    redirector_t *redirect = NULL;
    int k;
    if (!do_redirect)
	return;
    for (k = 0; k < NRedirectors; k++) {
	redirect = *(redirect_child_table + k);
	if (!(redirect->flags & REDIRECT_FLAG_ALIVE))
	    continue;
	if (redirect->flags & REDIRECT_FLAG_BUSY)
	    continue;
	if (redirect->flags & REDIRECT_FLAG_CLOSING)
	    continue;
	debug(29, 3, "redirectShutdownServers: closing redirector #%d, FD %d\n",
	    redirect->index + 1, redirect->fd);
	comm_close(redirect->fd);
	redirect->flags |= REDIRECT_FLAG_CLOSING;
    }
}


int redirectUnregister(url, fd)
     char *url;
     int fd;
{
    redirector_t *redirect = NULL;
    redirectStateData *r = NULL;
    struct redirectQueueData *rq = NULL;
    int k;
    int n = 0;
    if (!do_redirect)
	return 0;
    debug(29, 3, "redirectUnregister: FD %d '%s'\n", fd, url);
    for (k = 0; k < NRedirectors; k++) {
	redirect = *(redirect_child_table + k);
	if ((r = redirect->redirectState) == NULL)
	    continue;
	if (r->fd != fd)
	    continue;
	if (strcmp(r->orig_url, url))
	    continue;
	debug(29, 3, "redirectUnregister: Found match\n");
	r->handler = NULL;
	n++;
    }
    for (rq = redirectQueueHead; rq; rq = rq->next) {
	if ((r = rq->redirectState) == NULL)
	    continue;
	if (r->fd != fd)
	    continue;
	if (strcmp(r->orig_url, url))
	    continue;
	debug(29, 3, "redirectUnregister: Found match.\n");
	r->handler = NULL;
	n++;
    }
    debug(29, 3, "redirectUnregister: Unregistered %d handlers\n", n);
    return n;
}

void redirectStats(sentry)
     StoreEntry *sentry;
{
    int k;
    storeAppendPrintf(sentry, open_bracket);
    storeAppendPrintf(sentry, "{Redirector Statistics:}\n");
    storeAppendPrintf(sentry, "{requests: %d}\n",
	RedirectStats.requests);
    storeAppendPrintf(sentry, "{replies: %d}\n",
	RedirectStats.replies);
    storeAppendPrintf(sentry, "{queue length: %d}\n",
	RedirectStats.queue_size);
    storeAppendPrintf(sentry, "{avg service time: %d msec}\n",
	RedirectStats.avg_svc_time);
    storeAppendPrintf(sentry, "{number of redirectors: %d}\n",
	NRedirectors);
    storeAppendPrintf(sentry, "{use histogram:}\n");
    for (k = 0; k < NRedirectors; k++) {
	storeAppendPrintf(sentry, "{    redirector #%d: %d}\n",
	    k + 1,
	    RedirectStats.use_hist[k]);
    }
    storeAppendPrintf(sentry, close_bracket);
}

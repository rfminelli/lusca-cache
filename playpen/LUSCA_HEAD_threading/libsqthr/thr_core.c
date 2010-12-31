/*
 * $Id$
 *
 * DEBUG: section 88    Thread Core Library
 *
 */

#ifndef _REENTRANT
#error "_REENTRANT MUST be defined to build the squid thread core support."
#endif

#include "../include/config.h"

#include	<pthread.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/uio.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<dirent.h>
#include	<signal.h>
#if HAVE_SCHED_H
#include	<sched.h>
#endif
#include	<string.h>

/* this is for sqinet.h, which shouldn't be needed in here */
#include	<netinet/in.h>
#include 	<sys/socket.h>

#include "../include/util.h"
#include "../include/Array.h"
#include "../include/Stack.h"

#include "../libcore/dlink.h"
#include "../libcore/varargs.h"
#include "../libcore/tools.h"
#include "../libcore/gb.h"
#include "../libcore/kb.h"

#include "../libsqdebug/debug.h"

#include "../libmem/MemPool.h"
#include "../libmem/MemBufs.h"
#include "../libmem/MemBuf.h"

/*
 * This is for comm.h, which includes stuff that should be in the filedescriptor
 * related code. Sigh. That should be fixed.
 */
#include "../libsqinet/sqinet.h"

#include "../libiapp/fd_types.h"
#include "../libiapp/comm_types.h"
#include "../libiapp/iapp_ssl.h"
#include "../libiapp/comm.h"
#include "../libiapp/disk.h"

#include "thr_core.h"


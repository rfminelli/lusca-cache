#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>

#include "../include/config.h"
/*
 * The bulk of the following are because the disk code requires
 * the memory code, which requires everything else.
 */
#include "../include/Array.h"
#include "../include/Stack.h"
#include "../libcore/gb.h"
#include "../libcore/kb.h"
#include "../libcore/varargs.h"
#include "../libmem/MemPool.h"
#include "../libmem/MemBufs.h"
#include "../libmem/MemBuf.h"
#include "../libmem/MemStr.h"
#include "../libcb/cbdata.h"

#include "event.h"
#include "iapp_ssl.h"
#include "comm.h"
#include "mainloop.h"


/*
 * Configure the libraries required to bootstrap iapp.
 */
void
iapp_init(void)
{
	memPoolInit();
	memBuffersInit();
	memStringInit();
	cbdataInit();
	eventInit();
	comm_init();
	comm_select_init();
}

int
iapp_runonce(int msec)
{
	int loop_delay;

	eventRun();
	loop_delay = eventNextTime();
	if (loop_delay < 0)
		loop_delay = 0;
	if (loop_delay > msec)
		loop_delay = msec;
	return comm_select(loop_delay);
}

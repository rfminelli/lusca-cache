
/*
 * $Id$
 *
 * DEBUG: section 41    Event Processing
 * AUTHOR: Henrik Nordstrom
 *
 * SQUID Internet Object Cache  http://squid.nlanr.net/Squid/
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

/* The list of event processes */
struct ev_entry {
    EVH *func;
    void *arg;
    const char *name;
    time_t when;
    struct ev_entry *next;
    int weight;
};

static struct ev_entry *tasks = NULL;
static OBJH eventDump;

void
eventAdd(const char *name, EVH * func, void *arg, time_t when, int weight)
{
    struct ev_entry *event = xcalloc(1, sizeof(struct ev_entry));
    struct ev_entry **E;
    event->func = func;
    event->arg = arg;
    event->name = name;
    event->when = squid_curtime + when;
    event->weight = weight;
    if (NULL != arg)
        cbdataLock(arg);
    debug(41, 7) ("eventAdd: Adding '%s', in %d seconds\n", name, (int) when);
    /* Insert after the last event with the same or earlier time */
    for (E = &tasks; *E; E = &(*E)->next) {
	if ((*E)->when > event->when)
	    break;
    }
    event->next = *E;
    *E = event;
}

/* same as eventAdd but adds a random offset within +-1/3 of delta_ish */
void
eventAddIsh(const char *name, EVH * func, void *arg, time_t delta_ish, int weight)
{
    if (delta_ish >= 3) {
	const time_t two_third = (2 * delta_ish) / 3;
	delta_ish = two_third + (squid_random() % two_third);
    }
    eventAdd(name, func, arg, delta_ish, weight);
}

void
eventDelete(EVH * func, void *arg)
{
    struct ev_entry **E;
    struct ev_entry *event;
    for (E = &tasks; (event = *E) != NULL; E = &(*E)->next) {
	if (event->func != func)
	    continue;
	if (event->arg != arg)
	    continue;
	*E = event->next;
	if (NULL != event->arg)
	    cbdataUnlock(event->arg);
	xfree(event);
	return;
    }
    debug_trap("eventDelete: event not found");
}

void
eventRun(void)
{
    struct ev_entry *event = NULL;
    EVH *func;
    void *arg;
    int weight = 0;
    while (0 == weight) {
        if ((event = tasks) == NULL)
	    break;
        if (event->when > squid_curtime)
	    break;
        func = event->func;
        arg = event->arg;
        event->func = NULL;
        event->arg = NULL;
        tasks = event->next;
        if (NULL != arg) {
            int valid = cbdataValid(arg);
            cbdataUnlock(arg);
            if (!valid)
	        return;
        }
        weight += event->weight;
        debug(41, 7) ("eventRun: Running '%s'\n", event->name);
        func(arg);
        safe_free(event);
    }
}

time_t
eventNextTime(void)
{
    if (!tasks)
	return (time_t) 10;
    return tasks->when - squid_curtime;
}

void
eventInit(void)
{
    cachemgrRegister("events",
	"Event Queue",
	eventDump, 0);
}

static void
eventDump(StoreEntry * sentry)
{
    struct ev_entry *e = tasks;
    storeAppendPrintf(sentry, "%s\t%s\n",
	"Operation",
	"Next Execution");
    while (e != NULL) {
	storeAppendPrintf(sentry, "%s\t%d seconds\n",
	    e->name, (int) (e->when - squid_curtime));
	e = e->next;
    }
}

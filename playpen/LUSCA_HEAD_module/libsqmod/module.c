#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>

#include "../libcore/mem.h"
#include "../libcore/dlink.h"
#include "../libcore/varargs.h"

#include "../libsqdebug/debug.h"

#include "module.h"

dlink_list module_list;

int
module_init(void)
{
	bzero(&module_list, sizeof(module_list));
}

module_t *
module_lookup_bypath(const char *path)
{
	dlink_node *n = module_list.head;
	module_t *m;

	for (n = module_list.head; n != NULL; n = n->next) {
		/* XXX strcmp? */
		m = n->data;
		if (strcmp(path, m->mod_path) == 0)
			return m;
	}
	return NULL;
}

/*
 * Stuff to look at
 *
 * - dlopen() / dlclose() / dlsym()
 * - getting a handle to the module "object" to start doing things with it
 * - for now, ignore module "unloading" at runtime. Thats a hairier problem
 *   to try and solve. It should be solved later on, at least for people
 *   who want to be able to make Lusca reload updated modules during
 *   reconfigure.
 */

module_t *
module_register(const char *path)
{
	module_t *m;

	m = xxmalloc(sizeof(*m));
	if (! m)
		return NULL;

	debug(85, 1) ("module_register: loading %s\n", path);
	
	m->dl_handle = dlopen(path, RTLD_LAZY);
	if (! m->dl_handle) {
		/* XXX this should really log/return something */
		xxfree(m);
		debug(85, 1) ("module_register: failed: %s: %s\n", path, dlerror());
		return NULL;
	}

	/* Lookup the init symbol */
	m->mod_init = dlsym(m->dl_handle, "moduleInitFunc");
	if (! m->mod_init) {
		/* XXX this should really log/return something */
		xxfree(m);
		debug(85, 1) ("module_register: failed symbol lookup: %s: %s\n", path, dlerror());
		return NULL;
	}

	/* Set the module state */
	m->state = MOD_LOADED;

	/* Add it to the list */
	dlinkAddTail(m, &m->n, &module_list);

	/* Return success */
	debug(85, 1) ("module_register: %s: successful; state MOD_LOADED\n", path);
	return m;
}

void
module_setup(void)
{
	dlink_node *n;
	module_t *m;
	int r;

	debug(85, 1) ("module_setup: beginning\n");

	for (n = module_list.head; n != NULL; n = n->next) {
		if (m->state != MOD_LOADED) {
			debug(85, 1) ("module_register: %p (%s): skipping; state = %d\n",
			    m, m->mod_path, m->state);
			continue;
		}
		r = m->mod_init(m);
		debug(85, 1) ("module_register: %p (%s): mod_init returned %d\n", m, m->mod_path, r);
	}
}

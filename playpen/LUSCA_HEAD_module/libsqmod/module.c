#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dlfcn.h>

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
int
module_load(const char *path)
{
	void *handle = NULL;
	
	handle = dlopen(path, RTLD_LAZY);
	if (! handle) {
		return -1;
	}
}

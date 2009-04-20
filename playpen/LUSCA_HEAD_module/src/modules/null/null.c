#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

#include "libcore/dlink.h"

#include "libsqmod/module.h"

int
moduleInitFunc(module_t *module)
{
	return 1;
}


#include "squid.h"

void
hierarchyLogEntryCopy(HierarchyLogEntry *dst, HierarchyLogEntry *src)
{
	memcpy(dst, src, sizeof(HierarchyLogEntry));
}

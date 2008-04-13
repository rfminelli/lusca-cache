#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "debug.h"

int debugLevels[MAX_DEBUG_SECTIONS];
int _db_level;


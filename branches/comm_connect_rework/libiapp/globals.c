#include <stdio.h>

#include "globals.h"

int shutting_down = 0;
int opt_reuseaddr = 1;
int iapp_tcpRcvBufSz = 0;
int iapp_incomingRate;
const char * iapp_useAcceptFilter = NULL;

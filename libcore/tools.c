#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tools.h"

double
toMB(size_t size)
{
    return ((double) size) / MB;
}
 
size_t
toKB(size_t size)
{
    return (size + 1024 - 1) / 1024;
}

const char *
xinet_ntoa(const struct in_addr addr)
{
    return inet_ntoa(addr);
}


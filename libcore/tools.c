#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "tools.h"

double current_dtime;
time_t squid_curtime = 0;
struct timeval current_time;


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

time_t
getCurrentTime(void)
{
#if GETTIMEOFDAY_NO_TZP
    gettimeofday(&current_time);
#else
    gettimeofday(&current_time, NULL);
#endif
    current_dtime = (double) current_time.tv_sec +
        (double) current_time.tv_usec / 1000000.0;
    return squid_curtime = current_time.tv_sec;
}


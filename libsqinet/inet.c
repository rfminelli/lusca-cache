#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const char *
xinet_ntoa(const struct in_addr addr)
{
    return inet_ntoa(addr);
}

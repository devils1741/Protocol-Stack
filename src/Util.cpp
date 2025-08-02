#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>

std::string convert_uint32_to_ip(uint32_t ip)
{
    struct in_addr addr;
    addr.s_addr = ip;
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == nullptr)
    {
        return {};
    }
    return std::string(buf);
}

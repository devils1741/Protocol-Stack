#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <memory.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdint>

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

std::string sockaddr_in_to_string(const struct sockaddr_in &addr)
{
    char buffer[INET_ADDRSTRLEN + 10]; // 足够存储 IP 地址和端口号
    memset(buffer, 0, sizeof(buffer));

    // 将 IP 地址从网络字节序转换为字符串
    inet_ntop(AF_INET, &addr.sin_addr, buffer, INET_ADDRSTRLEN);

    // 将端口号从网络字节序转换为主机字节序
    uint16_t port = ntohs(addr.sin_port);

    // 拼接 IP 地址和端口号
    snprintf(buffer, sizeof(buffer), "%s:%d", buffer, port);

    return std::string(buffer);
}

std::string macAddressToString(const uint8_t *mac, size_t length)
{
    if (length != 6)
    {
        throw std::invalid_argument("MAC address must be 6 bytes long");
    }
    std::ostringstream oss;
    for (size_t i = 0; i < length; ++i)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
        if (i < length - 1)
        {
            oss << ":";
        }
    }
    return oss.str();
}
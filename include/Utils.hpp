#ifndef UTILS_HPP
#define UTILS_HPP
#include <cstdint>


string convert_uint32_to_ip(uint32_t ip);
std::string sockaddr_in_to_string(const struct sockaddr_in &addr);
std::string macAddressToString(const uint8_t *mac, size_t length);
#endif  
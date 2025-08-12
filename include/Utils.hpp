#ifndef UTILS_HPP
#define UTILS_HPP
#include <cstdint>


string convert_uint32_to_ip(uint32_t ip);
std::string sockaddr_in_to_string(const struct sockaddr_in &addr);
  
#endif  
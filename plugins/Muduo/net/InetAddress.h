#pragma once

#include <arpa/inet.h>

#include <string>

namespace Muduo {
class InetAddress {
 public:
  explicit InetAddress(uint16_t port = 0);
  InetAddress(const std::string& ip, uint16_t port);
  explicit InetAddress(const struct sockaddr_in& addr);
  ~InetAddress() = default;
  std::string toIp() const;
  std::string toIpPort() const;
  std::string toHostPort() const;
  const struct sockaddr_in& getSockAddrInet() const;
  void setSockAddrInet(const struct sockaddr_in& addr);
  uint32_t ipNetEndian() const;
  uint16_t portNetEndian() const;

 protected:
  struct sockaddr_in addr_ {};
};
}  // namespace Muduo
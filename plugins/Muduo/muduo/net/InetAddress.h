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
  [[nodiscard]] std::string toIp() const;
  [[nodiscard]] std::string toIpPort() const;
  [[nodiscard]] const struct sockaddr_in& getSockAddrInet() const;
  void setSockAddrInet(const struct sockaddr_in& addr);
  [[nodiscard]] uint32_t ipNetEndian() const;
  [[nodiscard]] uint16_t portNetEndian() const;

 protected:
  struct sockaddr_in addr_{};
};
}  // namespace Muduo
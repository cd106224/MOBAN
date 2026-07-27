#include "InetAddress.h"

#include <cstring>

#include "socketOps.h"

namespace Muduo {
InetAddress::InetAddress(uint16_t port) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = htonl(INADDR_ANY);
  addr_.sin_port = htons(port);
}

InetAddress::InetAddress(const std::string &ip, uint16_t port) {
  memset(&addr_, 0, sizeof(addr_));
  sockets::fromIpPort(ip.c_str(), port, &addr_);
}

InetAddress::InetAddress(const sockaddr_in &addr) : addr_(addr) {}

std::string InetAddress::toIp() const {
  char buf[32]{};
  sockets::toIp(buf, sizeof(buf), addr_);
  return buf;
}

std::string InetAddress::toIpPort() const {
  char buf[32];
  sockets::toIpPort(buf, sizeof(buf), addr_);
  return buf;
}

const struct sockaddr_in &InetAddress::getSockAddrInet() const { return addr_; }

void InetAddress::setSockAddrInet(const sockaddr_in &addr) { addr_ = addr; }

uint32_t InetAddress::ipNetEndian() const { return addr_.sin_addr.s_addr; }

uint16_t InetAddress::portNetEndian() const { return addr_.sin_port; }

}  // namespace Muduo
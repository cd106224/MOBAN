#pragma once

#include <arpa/inet.h>

namespace Muduo::sockets {
void fromIpPort(const char *ip, uint16_t port, struct sockaddr_in *addr);
void toIp(char *buf, size_t size, const struct sockaddr_in &addr);
void toIpPort(char *buf, size_t size, const struct sockaddr_in &addr);
}  // namespace Muduo::sockets
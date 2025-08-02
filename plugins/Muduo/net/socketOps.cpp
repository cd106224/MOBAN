#include "socketOps.h"

#include <log/logging.h>
#include <unistd.h>

namespace Muduo::sockets {
void fromIpPort(const char *ip, uint16_t port, struct sockaddr_in *addr) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
    LOG_ERROR("sockets:fromIpPort");
    exit(EXIT_FAILURE);
  }
}

void toIp(char *buf, size_t size, const struct sockaddr_in &addr) {
  assert(size >= INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &addr.sin_addr, buf, static_cast<socklen_t>(size));
}

void toIpPort(char *buf, size_t size, const struct sockaddr_in &addr) {
  char host[INET_ADDRSTRLEN] = "INVALID";
  toIp(host, sizeof(host), addr);
  const uint16_t port = ntohs(addr.sin_port);
  snprintf(buf, size, "%s:%u", host, port);
}
}  // namespace Muduo::sockets
#include "socket.h"

#include <netinet/tcp.h>

#include <cstring>

#include "InetAddress.h"
#include "socketOps.h"

namespace Muduo {
Socket::Socket(int sockfd) : sockfd_(sockfd) {}

Socket::~Socket() { sockets::close(sockfd_); }

int Socket::fd() const { return sockfd_; }

void Socket::bindAddress(const InetAddress& localaddr) const {
  sockets::bindOrDie(sockfd_, localaddr.getSockAddrInet());
}

void Socket::listen() const { sockets::listenOrDie(sockfd_); }

int Socket::accept(InetAddress* peerAddr) const {
  struct sockaddr_in addr{};
  memset(&addr, 0, sizeof(addr));
  const auto connfd = sockets::accept(sockfd_, &addr);
  if (connfd >= 0) {
    peerAddr->setSockAddrInet(addr);
  }
  return connfd;
}

void Socket::shutdownWrite() const { sockets::shutdownWrite(sockfd_); }

void Socket::setTcpNoDelay(bool on) const {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on) const {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on) const {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

}  // namespace Muduo
#include "socketOps.h"

#include <fcntl.h>
#include <log/logging.h>
#include <unistd.h>

namespace Muduo::sockets {

int createNonblockingOrDie() {
  int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    LOG_ERROR("sockets::createNonblockingOrDie Error!");
    exit(EXIT_FAILURE);
  }
  setNonblockAndCloseOnExec(sockfd);
  return sockfd;
}

void setNonblockAndCloseOnExec(int sockfd) {
  // non-block
  int flags = ::fcntl(sockfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int ret = ::fcntl(sockfd, F_SETFL, flags);
  (void)ret;

  // close-on-exec
  flags = ::fcntl(sockfd, F_GETFL, 0);
  flags |= O_CLOEXEC;
  ret = ::fcntl(sockfd, F_SETFL, flags);
  (void)ret;
}

void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
    LOG_ERROR("sockets:fromIpPort");
    exit(EXIT_FAILURE);
  }
}

void toIp(char* buf, size_t size, const struct sockaddr_in& addr) {
  assert(size >= INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &addr.sin_addr, buf, static_cast<socklen_t>(size));
}

void toIpPort(char* buf, size_t size, const struct sockaddr_in& addr) {
  char host[INET_ADDRSTRLEN] = "INVALID";
  toIp(host, sizeof(host), addr);
  const uint16_t port = ntohs(addr.sin_port);
  snprintf(buf, size, "%s:%u", host, port);
}

void close(int sockfd) {
  if (::close(sockfd) < 0) {
    LOG_ERROR("sockets::close");
    exit(EXIT_FAILURE);
  }
}

void bindOrDie(int sockfd, const struct sockaddr_in& addr) {
  const auto ret = ::bind(
      sockfd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));
  if (ret < 0) {
    LOG_ERROR("sockets::bindOrDie");
    exit(EXIT_FAILURE);
  }
}

void listenOrDie(int sockfd) {
  if (const auto ret = ::listen(sockfd, SOMAXCONN); ret < 0) {
    LOG_ERROR("sockets::listenOrDie");
    exit(EXIT_FAILURE);
  }
}

int accept(int sockfd, struct sockaddr_in* addr) {
  socklen_t addrlen = sizeof(*addr);
  int connfd = ::accept(sockfd, reinterpret_cast<sockaddr*>(addr), &addrlen);
  setNonblockAndCloseOnExec(connfd);
  if (connfd < 0) {
    int savedErrno = errno;
    LOG_ERROR("socket::accept");
    switch (savedErrno) {
      case EAGAIN:        // 当前没有可用的连接请求
      case ECONNABORTED:  // 连接被客户端中止
      case EINTR:         // 系统调用被信号中断
      case EPROTO:        // 协议错误
      case EPERM:         // 操作不被允许，防火墙或者权限之类
      case EMFILE:        // per-process limit of open file desctiptor
        // expected errors
        errno = savedErrno;
        break;
      case EBADF:       // 无效的文件描述符
      case EFAULT:      // 地址参数无效,通常是内存管理错误
      case EINVAL:      // 套接字状态无效,通常是套接字配置错误
      case ENFILE:      // 系统级文件描述符达到上限
      case ENOBUFS:     // 内核缓冲区不足,通常是系统资源耗尽
      case ENOMEM:      // 内存不足
      case ENOTSOCK:    // 文件不是套接字
      case EOPNOTSUPP:  // 套接字类型不支持accept操作,套接字类型配置错误
        // unexpected errors
        LOG_ERROR("unexpected error of ::accept {}", savedErrno);
        exit(EXIT_FAILURE);
      default: {
        LOG_ERROR("unexpected error of ::accept {}", savedErrno);
        exit(EXIT_FAILURE);
      }
    }
  }
  return connfd;
}

void shutdownWrite(int sockfd) {
  if (::shutdown(sockfd, SHUT_WR) < 0) {
    LOG_ERROR("sockets::shutdownWrite");
  }
}

int connect(int sockfd, const struct sockaddr_in& addr) {
  return ::connect(sockfd, reinterpret_cast<const sockaddr*>(&addr),
                   sizeof(addr));
}

int getSocketError(int sockfd) {
  int optval;
  socklen_t optlen = sizeof(optval);
  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return errno;
  } else {
    return optval;
  }
}

// 自连接是指(sourceIP, sourcePort) = (destIP, destPort)
// 自连接发生的原因:
// 客户端在发起connect的时候，没有bind(2)
// 客户端与服务器端在同一台机器，即sourceIP = destIP，
// 服务器尚未开启，即服务器还没有在destPort端口上处于监听
// 就有可能出现自连接，这样，服务器也无法启动了

bool isSelfConnect(int sockfd) {
  struct sockaddr_in localaddr = getLocalAddr(sockfd);
  struct sockaddr_in peeraddr = getPeerAddr(sockfd);
  return localaddr.sin_addr.s_addr == peeraddr.sin_addr.s_addr &&
         localaddr.sin_port == peeraddr.sin_port;
}

sockaddr_in getLocalAddr(int sockfd) {
  sockaddr_in localaddr{};
  memset(&localaddr, 0, sizeof(localaddr));
  socklen_t addrlen = sizeof(localaddr);
  if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&localaddr), &addrlen) <
      0) {
    LOG_ERROR("sockets::getLocalAddr");
    exit(EXIT_FAILURE);
  }
  return localaddr;
}

sockaddr_in getPeerAddr(int sockfd) {
  sockaddr_in peeraddr{};
  memset(&peeraddr, 0, sizeof(peeraddr));
  socklen_t addrlen = sizeof(peeraddr);
  if (::getpeername(sockfd, reinterpret_cast<sockaddr*>(&peeraddr), &addrlen) <
      0) {
    LOG_ERROR("sockets::getPeerAddr");
    exit(EXIT_FAILURE);
  }
  return peeraddr;
}

}  // namespace Muduo::sockets
#pragma once

#include <utilities/noncopyable.h>

namespace Muduo {

class InetAddress;

class Socket final : noncopyable {
 public:
  explicit Socket(int sockfd);
  ~Socket() override;
  [[nodiscard]] int fd() const;
  void bindAddress(const InetAddress& localaddr) const;
  void listen() const;
  int accept(InetAddress* peerAddr) const;
  void shutdownWrite() const;

  /**
   * @brief  Nagle算法可以一定程度上避免网络拥塞
   *         TCP_NODELAY选项可以禁用Nagle算法
   *         禁用Nagle算法，可以避免连续发包出现延迟，这对于编写低延迟的网络服务很重要
   * @param  on
   */
  void setTcpNoDelay(bool on) const;

  /**
   * @brief  Enable/disable SO_REUSEADDR
   * @param  on
   */
  void setReuseAddr(bool on) const;

  /**
   * @brief  TCP
   * keepalive是指定期探测连接是否存在，如果应用层有心跳的话，这个选项不是必需要设置的
   * @param  on
   */
  void setKeepAlive(bool on) const;

 protected:
  int sockfd_;
};
}  // namespace Muduo
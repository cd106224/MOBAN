#pragma once

/**
 * @brief   Acceptor的封装
 * @author  guogang
 */

#include <boost/core/noncopyable.hpp>
#include <functional>

#include "Channel.h"
#include "socket.h"

namespace Muduo {
class EventLoop;
class InetAddress;

class Acceptor : public boost::noncopyable {
 public:
  using NewConnectionCallback =
      std::function<void(int sockfd, const InetAddress&)>;
  Acceptor(EventLoop* loop, const InetAddress& listenAddr);
  ~Acceptor();
  void setNewConnectionCallback(const NewConnectionCallback& cb);
  bool listening() const;
  void listen();

 protected:
  void handleRead();

 protected:
  EventLoop* loop_;
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
  bool listening_;
  int idlefd_;
};

}  // namespace Muduo
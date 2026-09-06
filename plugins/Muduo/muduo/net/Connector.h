#pragma once

#include <utilities/noncopyable.h>

#include <functional>
#include <memory>

#include "InetAddress.h"

namespace Muduo {
class EventLoop;
class Channel;

class Connector : public noncopyable, std::enable_shared_from_this<Connector> {
 public:
  using NewConnectionCallback = std::function<void(int sockfd)>;

  Connector(EventLoop* loop, const InetAddress& serverAddr);
  ~Connector();
  void setNewConnectionCallback(const NewConnectionCallback& cb);
  void start();    // can be call in any thread
  void restart();  // must be call in loop thread
  void stop();     // can be call in any thread
  const InetAddress& serverAddr();

 protected:
  enum States { KDisconnected, Kconnecting, Kconnected };
  static const int KMaxRetryDelayMs = 30 * 1000;  // 30s,最大延迟时间
  static const int KInitRetryDelayMs =
      500;  // 500ms,初始状态,连接不上,500ms后续联
  void setState(States s);
  void startInLoop();
  void stopInLoop();
  void connect();
  void connecting(int sockfd);
  void handleWrite();
  void handleError();
  void retry(int sockfd);
  int removeAndResetChannel();
  void resetChannel();

 protected:
  EventLoop* loop_;
  InetAddress serverAddr_;
  bool connect_;
  States state_;
  std::unique_ptr<Channel> channel_;
  NewConnectionCallback newConnectionCallback_;
  int retryDelayMs_;
};
}  // namespace Muduo
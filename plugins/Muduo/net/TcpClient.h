#pragma once

#include <memory>

#include "EventLoop.h"
#include "TcpConnection.h"

namespace Muduo {
class Connector;

class TcpClient : public noncopyable {
  using ConnectorPtr = std::shared_ptr<Connector>;
  TcpClient(EventLoop* loop, const InetAddress& serverAddr, std::string name);
  ~TcpClient();
  void connect();
  void disconnect();
  void stop();
  TcpConnectionPtr connection() const;
  bool retry() const;
  void enableRetry();
  void setConnectionCallback(const ConnectionCallback& cb);
  void setMessageCallback(const MessageCallback& cb);
  void setWriteCompleteCallback(const WriteCompleteCallback& cb);

 protected:
  void newConnection(int sockfd);  // not thread safe,but in loop
  void removeConnection(const TcpConnectionPtr& conn);

 protected:
  EventLoop* loop_;
  ConnectorPtr connector_;  // 用于主动发起连接
  const std::string name_;  // client name
  ConnectionCallback ConnectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  bool retry_;
  std::atomic<bool> connect_;  // atomic
  int32_t nextConnId_;         // name+nectConnId 标识一个连接
  mutable std::mutex mutex_;
  TcpConnectionPtr connection_;
};

}  // namespace Muduo
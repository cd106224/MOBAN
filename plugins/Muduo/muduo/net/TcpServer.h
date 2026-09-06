#pragma once

#include <utilities/noncopyable.h>

#include <functional>
#include <map>

#include "InetAddress.h"
#include "callbacks.h"

namespace Muduo {
class Acceptor;
class EventLoop;
class EventLoopThreadPool;

class TcpServer : public noncopyable {
 public:
  using ThreadInitCallback = std::function<void(EventLoop*)>;
  TcpServer(EventLoop* loop, const InetAddress& listenAddr,
            const std::string& nameArg);
  ~TcpServer();
  const std::string& hostport();
  const std::string& name();
  // 返回监听socket绑定的本地地址(监听0端口时为内核分配的实际端口)。
  [[nodiscard]] InetAddress listenAddr() const;
  void setThreadNum(int numThreads);
  void setThreadInitCallback(const ThreadInitCallback& cb);
  void start();
  /**
   * @brief 设置连接到来或者连接关闭回调函数
   * @param cb
   */
  void setConnectionCallback(const ConnectionCallback& cb);
  void setMessageCallback(const MessageCallback& cb);
  void setWriteCompleteCallback(const WriteCompleteCallback& cb);

 protected:
  // not thread safe,but in loop
  void newConnection(int sockfd, const InetAddress& peerAddr);
  // thread safe
  void removeConnection(const TcpConnectionPtr& conn);
  // not thread safe,but in loop
  void removeConnectionInLoop(const TcpConnectionPtr& conn);
  using ConnectionMap = std::map<std::string, TcpConnectionPtr>;

 protected:
  EventLoop* loop_;
  const std::string hostport_;  // 服务端口
  const std::string name_;      // 服务名
  std::unique_ptr<Acceptor> acceptor_;
  std::unique_ptr<EventLoopThreadPool> threadPool_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  // IO线程池中的线程在进入事件循环前,会回调此函数
  ThreadInitCallback threadInitCallback_;
  bool started_;
  int nextConnId_;             // 下一个连接id
  ConnectionMap connections_;  // 连接链表
};
}  // namespace Muduo
#pragma once

#include <log/logging.h>
#include <utilities/noncopyable.h>

#include <any>
#include <memory>

#include "InetAddress.h"
#include "buffer.h"
#include "callbacks.h"

namespace Muduo {
class Channel;
class EventLoop;
class Socket;

class TcpConnection : public noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
 public:
  TcpConnection(EventLoop* loop, std::string name, int sockfd,
                const InetAddress& localAddr, const InetAddress& peerAddr);
  ~TcpConnection();

  EventLoop* getLoop();
  const std::string& name();
  const InetAddress& localAddress();
  const InetAddress& peerAddress();
  bool connected();
  void send(const void* message, size_t len);
  void send(std::string& message);
  void send(Buffer* message);
  void shutdown();
  void forceClose();
  void setTcpNoDelay(bool on);
  void setContext(const std::any& context);
  const std::any& getContext() const;
  std::any* getMutableContext();
  void setConnectionCallback(const ConnectionCallback& cb);
  void setMessageCallback(const MessageCallback& cb);
  void setWriteCompleteCallback(const WriteCompleteCallback& cb);
  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb,
                                size_t highWaterMark);
  Buffer* inputBuffer();
  void setCloseCallback(const CloseCallback& cb);
  void connectEstablished();
  void connnectDestroyed();

 protected:
  enum StateE { KDisconnected, KConnecting, KConnected, KDisconnecting };
  friend struct ::fmt::formatter<StateE>;
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void forceCloseInLoop();
  void handleError();
  void sendInLoopByString(const std::string& message);
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();
  void setState(StateE s);

 protected:
  EventLoop* loop_;
  std::string name_;
  StateE state_;
  std::unique_ptr<Socket> socket_;
  std::unique_ptr<Channel> channel_;
  InetAddress localAddr_;
  InetAddress peerAddr_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  /**
   * 数据发送完毕回调函数,所有的用户数据都已经拷贝到内核缓冲区时回调此函数
   * outputBuffer_被清空也会回调此函数,可以理解为低水位标回调函数
   */
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;  // 高水位标回调函数
  CloseCallback closeCallback_;                  // 内部断开回调函数
  size_t highWaterMark_{};                       // 高水位标
  Buffer inputBuffer_;                           // 输入缓冲区
  Buffer outputBuffer_;                          // 输出缓冲区
  std::any context_;                             // 绑定一个未知类型的上下文对象
};
}  // namespace Muduo

template <>
struct fmt::formatter<Muduo::TcpConnection::StateE> {
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

  auto format(Muduo::TcpConnection::StateE s, fmt::format_context& ctx) const {
    std::string_view name;
    switch (s) {
      case Muduo::TcpConnection::KDisconnected:
        name = "KDisconnected";
        break;
      case Muduo::TcpConnection::KConnecting:
        name = "KConnecting";
        break;
      case Muduo::TcpConnection::KConnected:
        name = "KConnected";
        break;
      case Muduo::TcpConnection::KDisconnecting:
        name = "KDisconnecting";
        break;
      default:
        name = "Unknown";
        break;
    }
    return fmt::format_to(ctx.out(), "{}", name);
  }
};

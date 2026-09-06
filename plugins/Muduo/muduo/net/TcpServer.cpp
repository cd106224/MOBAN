#include "TcpServer.h"

#include <log/logging.h>

#include "Acceptor.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "TcpConnection.h"
#include "socketOps.h"

namespace Muduo {
TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr,
                     const std::string& nameArg)
    : loop_(loop),
      hostport_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr)),
      threadPool_(new EventLoopThreadPool(loop)),
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      started_(false),
      nextConnId_(1) {
  // Acceptor::handleRead函数会回调TcpServer::newConnection
  //_1对应的是socket文件描述符,_2对应的是对等方的地址InetAddr
  acceptor_->setNewConnectionCallback([this](auto&& PH1, auto&& PH2) {
    newConnection(std::forward<decltype(PH1)>(PH1),
                  std::forward<decltype(PH2)>(PH2));
  });
}

TcpServer::~TcpServer() {
  loop_->assertInLoopThread();
  LOG_INFO("TcpServer::~TcpServer[{}] destructing", name_);
  for (auto& v : connections_) {
    TcpConnectionPtr conn = v.second;
    v.second.reset();  // 释放当前所控制的对象,引用计数减一
    conn->getLoop()->runInLoop([conn] { conn->connnectDestroyed(); });
    conn.reset();
  }
}

const std::string& TcpServer::hostport() { return hostport_; }

const std::string& TcpServer::name() { return name_; }

InetAddress TcpServer::listenAddr() const { return acceptor_->listenAddr(); }

void TcpServer::start() {
  if (!started_) {
    started_ = true;
    threadPool_->start(threadInitCallback_);
  }
  if (!acceptor_->listening()) {
    loop_->runInLoop([capture0 = acceptor_.get()] { capture0->listen(); });
  }
}

void TcpServer::setConnectionCallback(const ConnectionCallback& cb) {
  connectionCallback_ = cb;
}

void TcpServer::setMessageCallback(const MessageCallback& cb) {
  messageCallback_ = cb;
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
  loop_->assertInLoopThread();
  // 按照轮叫的方式选择一个EventLoop
  EventLoop* ioLoop = threadPool_->getNextLoop();
  char buf[32];
  snprintf(buf, sizeof(buf), "%s:%d", hostport().c_str(), nextConnId_);
  ++nextConnId_;
  std::string connName = name_ + buf;
  LOG_INFO("TcpServer::newConnection [{}]-new connection [{}] from {}", name_,
           connName, peerAddr.toIpPort());
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  TcpConnectionPtr conn(
      new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
  connections_[connName] = conn;
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback([this](auto&& PH1) {
    removeConnection(std::forward<decltype(PH1)>(PH1));
  });
  ioLoop->runInLoop(
      [capture0 = conn.get()] { capture0->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
  loop_->runInLoop([this, conn] { removeConnectionInLoop(conn); });
}

void TcpServer::setThreadNum(int numThreads) {
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::setThreadInitCallback(const TcpServer::ThreadInitCallback& cb) {
  threadInitCallback_ = cb;
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
  loop_->assertInLoopThread();
  LOG_INFO("TcpServer::removeConnnection[{}]-connection:{}", name_,
           conn->name());
  size_t n = connections_.erase(conn->name());
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();
  ioLoop->queueInLoop([conn] {
    conn->connnectDestroyed();
  });  // 在此如果直接调用,conn就结束了自己的生命周期
}

void TcpServer::setWriteCompleteCallback(const WriteCompleteCallback& cb) {
  writeCompleteCallback_ = cb;
}

}  // namespace Muduo
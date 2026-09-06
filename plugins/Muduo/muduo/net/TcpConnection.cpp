#include "TcpConnection.h"

#include <log/logging.h>

#include "Channel.h"
#include "EventLoop.h"
#include "socket.h"
#include "socketOps.h"

namespace Muduo {
void defaultConnectionCallback(const TcpConnectionPtr& conn) {
  LOG_INFO("{}->{} is {}", conn->localAddress().toIpPort().c_str(),
           conn->peerAddress().toIpPort().c_str(),
           (conn->connected()) ? "UP" : "DOWN");
}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buf, Timestamp) {
  buf->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop* loop, std::string name, int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : loop_(loop),
      name_(std::move(name)),
      state_(KConnecting),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      context_(nullptr) {
  // 通道可读事件到来的时候,回调TcpConnection::handleRead,_1是事件发生时间
  channel_->setReadCallback(
      [this](const Timestamp& receiveTime) { handleRead(receiveTime); });
  // 通道可写事件到来的时候,回调TcpConnection::handleWrite
  channel_->setWriteCallback([this] { handleWrite(); });
  // 连接关闭,回调TcpConnection::handleClose
  channel_->setCloseCallback([this] { handleClose(); });
  // 发生错误,回调TcpConnection::handleError
  channel_->setErrorCallback([this] { handleError(); });
  LOG_INFO("TcpConnection::ctor[{}] at {} fd={}", name.c_str(),
           static_cast<const void*>(this), sockfd);
  socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
  LOG_INFO("TcpConnection::dtor[{}] at {} fd={}", name_,
           static_cast<const void*>(this), channel_->fd());
}

EventLoop* TcpConnection::getLoop() { return loop_; }

const std::string& TcpConnection::name() { return name_; }

const InetAddress& TcpConnection::localAddress() { return localAddr_; }

const InetAddress& TcpConnection::peerAddress() { return peerAddr_; }

bool TcpConnection::connected() { return state_ == KConnected; }

void TcpConnection::setConnectionCallback(const ConnectionCallback& cb) {
  connectionCallback_ = cb;
}

void TcpConnection::setMessageCallback(const MessageCallback& cb) {
  messageCallback_ = cb;
}

void TcpConnection::connectEstablished() {
  loop_->assertInLoopThread();
  assert(state_ == KConnecting);
  setState(KConnected);
  channel_->tie(shared_from_this());
  channel_->enableReading();  // TcpConnection对应的通道加入Poller进行关注
  connectionCallback_(shared_from_this());
}

void TcpConnection::handleRead(Timestamp receiveTime) {
  loop_->assertInLoopThread();
  int saveErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
  if (n > 0) {
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  } else if (n == 0) {
    handleClose();
  } else {
    errno = saveErrno;
    LOG_ERROR("handleRead():errno:{}", errno);
    handleError();
  }
}

// 内核发送缓冲区有空间了,回调此函数
void TcpConnection::handleWrite() {
  loop_->assertInLoopThread();
  if (channel_->isWriting()) {
    ssize_t n = ::write(channel_->fd(), outputBuffer_.peek(),
                        outputBuffer_.readableBytes());
    if (n > 0) {
      outputBuffer_.retrieve(static_cast<size_t>(n));
      if (outputBuffer_.readableBytes() == 0)  // 发送缓冲区已经清空
      {
        channel_->disableWriting();  // 停止关闭POLLLOUT事件,以免出现busyLoop
        if (writeCompleteCallback_) {
          loop_->queueInLoop([capture0 = shared_from_this()] {
            capture0->writeCompleteCallback_(capture0);
          });
        }
        // 发送缓冲区已经清空并且连接状态是KDisconnecting,要关闭连接
        if (state_ == KDisconnecting) {
          shutdownInLoop();  // 关闭连接
        }
      } else {
        LOG_INFO("I am going to write more data");
      }
    } else {
      // 写失败(如对端RST导致的EPIPE)只影响本连接,不应终止整个进程;
      // 连接会在随后读端事件的handleClose中被回收
      LOG_ERROR("TcpConnection::handleWrite");
    }
  } else {
    LOG_INFO("Connection fd= {} is down,no more writing", channel_->fd());
  }
}

void TcpConnection::setState(TcpConnection::StateE s) { state_ = s; }

void TcpConnection::connnectDestroyed() {
  loop_->assertInLoopThread();
  if (state_ == KConnected) {
    setState(KDisconnected);
    channel_->disableAll();
    connectionCallback_(shared_from_this());  // 用户的回调函数
  }
  channel_->remove();
}

void TcpConnection::setCloseCallback(const CloseCallback& cb) {
  closeCallback_ = cb;
}

void TcpConnection::handleClose() {
  loop_->assertInLoopThread();
  LOG_INFO("fd={},state={}", channel_->fd(), state_);
  assert(state_ == KConnected || state_ == KDisconnecting);
  setState(KDisconnected);
  channel_->disableAll();

  TcpConnectionPtr guardThis(shared_from_this());
  connectionCallback_(guardThis);  // 回调用户的函数
  closeCallback_(guardThis);
}

void TcpConnection::handleError() {
  int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR("TcpConnection::handleError[{}]-SO_ERROR:{}", name_, err);
}

// 线程安全,可以跨线程调用
void TcpConnection::send(const void* message, size_t len) {
  if (state_ == KConnected) {
    if (loop_->isInLoopThread()) {
      sendInLoop(message, len);
    } else {
      std::string nmessages(static_cast<const char*>(message), len);
      loop_->runInLoop([this, nmessages] { sendInLoopByString(nmessages); });
    }
  }
}

// 线程安全,可以跨线程调用
void TcpConnection::send(std::string& message) {
  if (state_ == KConnected) {
    if (loop_->isInLoopThread()) {
      sendInLoopByString(message);
    } else {
      loop_->runInLoop([this, message] { sendInLoopByString(message); });
    }
  }
}

// 线程安全,可以跨线程调用
void TcpConnection::send(Buffer* message) {
  if (state_ == KConnected) {
    if (loop_->isInLoopThread()) {
      sendInLoop(message->peek(), message->readableBytes());
      message->retrieveAll();
    } else {
      loop_->runInLoop([this, capture0 = message->retrieveAllAsString()] {
        sendInLoopByString(capture0);
      });
    }
  }
}

void TcpConnection::sendInLoopByString(const std::string& message) {
  sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void* message, size_t len) {
  loop_->assertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;  // 写失败(对端已断开)时置位,不再缓冲剩余数据
  if (state_ == KDisconnected) {
    LOG_ERROR("disconnected,give up writing");
    return;
  }
  // 通道没有在写数据并且发送缓冲区没有数据,直接Write
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
    nwrote = ::write(channel_->fd(), message, len);
    if (nwrote > 0) {
      remaining = len - static_cast<size_t>(nwrote);
      if (remaining == 0 && writeCompleteCallback_) {
        loop_->queueInLoop(
            std::bind(writeCompleteCallback_, shared_from_this()));
      }
    } else {
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG_ERROR("TcpConnection::sendInLoop");
        // 写失败(对端RST/断开)只影响本连接,不终止进程;丢弃本次未写完的数据
        if (errno == EPIPE || errno == ECONNRESET) {
          faultError = true;
        }
      }
    }
  }
  assert(remaining <= len);
  // 没有错误,并且还有未写完的少数据(说明内核发送缓冲区满,要将未写完的数据添加到output
  // buffer中);faultError时连接已不可写,不再把剩余数据挂到缓冲
  if (!faultError && remaining > 0) {
    LOG_INFO("I am going to write more data");
    size_t oldLen = outputBuffer_.readableBytes();
    // 如果超过highWriteMark,回调m_highWaterMarkCallback
    if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ &&
        highWaterMarkCallback_) {
      loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(),
                                   oldLen + remaining));
    }
    outputBuffer_.append(static_cast<const char*>(message) + nwrote, remaining);
    if (!channel_->isWriting()) {
      channel_->enableWriting();  // 关注POLLOUT事件
    }
  }
}

void TcpConnection::shutdown() {
  if (state_ == KConnected) {
    setState(KDisconnecting);
    loop_->runInLoop([this] { shutdownInLoop(); });
  }
}

void TcpConnection::shutdownInLoop() {
  loop_->assertInLoopThread();
  if (!channel_->isWriting()) {
    socket_->shutdownWrite();
  }
}

// 线程安全,可以跨线程调用
// 强制关闭连接:用于连接只剩最后一个引用(TcpClient析构)时,确保走完
// handleClose->closeCallback->removeChannel的完整回收流程,而不是裸析构
void TcpConnection::forceClose() {
  if (state_ == KConnected || state_ == KDisconnecting) {
    setState(KDisconnecting);
    loop_->queueInLoop([self = shared_from_this()] { self->forceCloseInLoop(); });
  }
}

void TcpConnection::forceCloseInLoop() {
  loop_->assertInLoopThread();
  if (state_ == KConnected || state_ == KDisconnecting) {
    handleClose();  // 模拟读到EOF,触发统一的关闭流程
  }
}

void TcpConnection::setTcpNoDelay(bool on) { socket_->setTcpNoDelay(on); }

Buffer* TcpConnection::inputBuffer() { return &inputBuffer_; }

void TcpConnection::setContext(const std::any& context) { context_ = context; }

const std::any& TcpConnection::getContext() const { return context_; }

std::any* TcpConnection::getMutableContext() { return &context_; }

void TcpConnection::setWriteCompleteCallback(const WriteCompleteCallback& cb) {
  writeCompleteCallback_ = cb;
}

void TcpConnection::setHighWaterMarkCallback(const HighWaterMarkCallback& cb,
                                             size_t highWaterMark) {
  highWaterMarkCallback_ = cb;
  highWaterMark_ = highWaterMark;
}

}  // namespace Muduo
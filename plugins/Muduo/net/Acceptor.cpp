#include "Acceptor.h"

#include <base/timestamp.h>
#include <fcntl.h>

#include <cassert>
#include <cerrno>

#include "EventLoop.h"
#include "InetAddress.h"
#include "socketOps.h"

namespace Muduo {
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie()),
      acceptChannel_(loop_, acceptSocket_.fd()),
      listening_(false),
      idlefd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
  assert(idlefd_ > 0);
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.bindAddress(listenAddr);
  acceptChannel_.setReadCallback(
      [this](const Timestamp&) { this->handleRead(); });
}

Acceptor::~Acceptor() {
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idlefd_);
}

void Acceptor::setNewConnectionCallback(const NewConnectionCallback& cb) {
  newConnectionCallback_ = cb;
}

void Acceptor::listen() {
  loop_->assertInLoopThread();
  listening_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading();
}

bool Acceptor::listening() const { return listening_; }

// idlefd_ 是应对 fd 耗尽（EMFILE）时的应急措施，核心是"宁可自己主动 accept
// 掉再关，也不能让连接堆在队列里触发死循环"。
void Acceptor::handleRead() {
  loop_->assertInLoopThread();
  InetAddress peerAddr(0);
  int connfd = acceptSocket_.accept(&peerAddr);
  if (connfd >= 0) {
    if (newConnectionCallback_) {
      newConnectionCallback_(connfd, peerAddr);
    } else {
      sockets::close(connfd);
    }
  } else {
    if (errno == EMFILE) {
      ::close(idlefd_);
      idlefd_ = ::accept(acceptSocket_.fd(), nullptr, nullptr);
      ::close(idlefd_);
      idlefd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

}  // namespace Muduo
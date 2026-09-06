#include "Connector.h"

#include <log/logging.h>

#include <cassert>
#include <cerrno>
#include <memory>

#include "EventLoop.h"
#include "InetAddress.h"
#include "socketOps.h"

namespace Muduo {
Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      serverAddr_(serverAddr),
      connect_(false),
      state_(KDisconnected),
      retryDelayMs_(KInitRetryDelayMs) {}

Connector::~Connector() { assert(!channel_); }

void Connector::setNewConnectionCallback(const NewConnectionCallback& cb) {
  newConnectionCallback_ = cb;
}

void Connector::start() {
  if (connect_) {  // 在linux下 atomic
    return;
  }
  connect_ = true;
  loop_->runInLoop([this]() { this->startInLoop(); });
}

void Connector::startInLoop() {
  loop_->assertInLoopThread();
  assert(state_ == KDisconnected);
  if (connect_) {
    connect();
  }
}

void Connector::stop() {
  connect_ = false;
  loop_->runInLoop([this] { this->stopInLoop(); });
}

void Connector::stopInLoop() {
  loop_->assertInLoopThread();
  if (state_ == Kconnecting) {
    setState(KDisconnected);
    int sockfd =
        removeAndResetChannel();  // 将通道从Poller中转出,并将channel清空
    // retry(sockfd);//这里并非重连,只是调用sockets::close(sockfd);
    sockets::close(sockfd);
  }
}

void Connector::connect() {
  int sockfd = sockets::createNonblockingOrDie();
  int ret = sockets::connect(sockfd, serverAddr_.getSockAddrInet());
  int saveError = (ret == 0) ? 0 : errno;
  switch (saveError) {
    case 0:
    case EINPROGRESS:  // 非阻塞套接字,未连接成功返回EINPROGRESS表示正在连接
    case EISCONN:      // 表示连接成功
      connecting(sockfd);
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd);  // 重连
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_ERROR("connect error in Connector::startInLoop!");
      sockets::close(sockfd);
      break;
    default:
      LOG_ERROR("unKnow error in Connector::startInLoop!");
      sockets::close(sockfd);
      break;
  }
}

void Connector::restart() {
  loop_->assertInLoopThread();
  setState(KDisconnected);
  retryDelayMs_ = KInitRetryDelayMs;
  connect_ = true;
  startInLoop();
}

const InetAddress& Connector::serverAddr() { return serverAddr_; }

void Connector::setState(States s) { state_ = s; }

void Connector::connecting(int sockfd) {
  setState(Kconnecting);
  assert(!channel_);
  channel_ = std::make_unique<Channel>(loop_, sockfd);
  channel_->setWriteCallback([this] { handleWrite(); });
  channel_->setErrorCallback([this] { handleError(); });
  channel_->enableWriting();  // 让Poller关注可写事件
}

int Connector::removeAndResetChannel() {
  channel_->disableAll();
  channel_->remove();  // 从poller移除关注
  int sockfd = channel_->fd();
  // Can't reset channel_ here, because we are inside Channel::handleEvent
  // 不能在这里重置channel_，因为正在调用Channel::handleEvent
  loop_->queueInLoop([this] { resetChannel(); });
  return sockfd;
}

void Connector::resetChannel() { channel_.reset(); }

void Connector::handleWrite() {
  if (state_ == Kconnecting) {
    int sockfd = removeAndResetChannel();  // 从poller中移除关注,并将Channel置空
    // sockfd可写,并不意味着连接一定建立成功
    // 还需要用getsockopt(sockfd, SOL_SOCKET, SO_ERROR, ...)再次确认一下
    int err = sockets::getSocketError(sockfd);
    if (err != 0) {
      retry(sockfd);                            // 重连
    } else if (sockets::isSelfConnect(sockfd))  // 自连接
    {
      retry(sockfd);  // 重连
    } else {
      // 连接成功
      setState(Kconnected);
      if (connect_) {
        newConnectionCallback_(sockfd);
      } else {
        sockets::close(sockfd);
      }
    }
  } else {
    assert(state_ == KDisconnected);
  }
}

void Connector::handleError() {
  assert(state_ == Kconnecting);
  int sockfd = removeAndResetChannel();  // 从Poller中移除关注,并将Channel移除
  int err = sockets::getSocketError(sockfd);
  LOG_ERROR("Connect HandleError():err:{}", err);
  retry(sockfd);
}

// 采用back-off策略重连，即重连时间逐渐延长，0.5s, 1s, 2s, ...直至30s
void Connector::retry(int sockfd) {
  sockets::close(sockfd);
  setState(KDisconnected);
  if (connect_) {
    // 注册一个定时操作,重连
    loop_->runAfter(retryDelayMs_ / 1000.0, [capture0 = shared_from_this()] {
      capture0->startInLoop();
    });  // 延长Connector的生命周期
    retryDelayMs_ = (retryDelayMs_ * 2) < KMaxRetryDelayMs ? (retryDelayMs_ * 2)
                                                           : KMaxRetryDelayMs;
  }
}

}  // namespace Muduo
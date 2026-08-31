#include "Channel.h"

#include <sys/poll.h>

#include <cassert>

#include "base/timestamp.h"
#include "log/logging.h"

namespace Muduo {
const int Channel::KNoneEvent = 0;
const int Channel::KReadEvent = POLLIN | POLLPRI;  // POLLPRI表示紧急数据
const int Channel::KWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      logHup_(true),
      tied_(false),
      eventHandling_(false) {}

Channel::~Channel() { assert(!eventHandling_); }

void Channel::tie(const std::shared_ptr<void>& obj) {
  tie_ = obj;
  tied_ = true;
}

void Channel::handleEvent(const Timestamp& receiveTime) {
  std::shared_ptr<void> guard;
  if (tied_) {
    guard = tie_.lock();
    if (guard) {
      handleEventWithGuard(receiveTime);
    }
  } else {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::setReadCallback(const ReadEventCallback& cb) {
  readCallback_ = cb;
}

void Channel::setWriteCallback(const EventCallback& cb) { writeCallback_ = cb; }

void Channel::setCloseCallback(const EventCallback& cb) { closeCallback_ = cb; }

void Channel::setErrorCallback(const EventCallback& cb) { errorCallback_ = cb; }

int Channel::fd() const { return fd_; }

int Channel::events() { return events_; }

void Channel::set_revents(int revt) { revents_ = revt; }

bool Channel::isNoneEvent() { return events_ == KNoneEvent; }

void Channel::enableReading() {
  events_ |= KReadEvent;
  update();
}

void Channel::enableWriting() {
  events_ |= KWriteEvent;
  update();
}

void Channel::disableWriting() {
  events_ &= ~KWriteEvent;
  update();
}

void Channel::disableAll() {
  events_ = KNoneEvent;
  update();
}

bool Channel::isWriting() { return events_ & KWriteEvent; }

int Channel::index() { return index_; }

void Channel::set_index(int idx) { index_ = idx; }

void Channel::doNotLogHup() { logHup_ = false; }

EventLoop* Channel::ownerLoop() { return loop_; }

// 调用这个函数之前确保调用了disableAll
void Channel::remove() {
  assert(isNoneEvent());
  // TODO:GG
}

void Channel::update() {
  // TODO:GG
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
  eventHandling_ = true;
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
    if (logHup_) {
      LOG_WARN("Channel::handle_event() POLLHUP");  // 对方断开
    }
    if (closeCallback_) {
      closeCallback_();
    }
  }

  if (revents_ & POLLNVAL) {  // 描述字不是一个打开的文件
    LOG_WARN("Channel::handle_event() POLLNVAL");
  }

  if (revents_ & (POLLERR | POLLNVAL)) {
    if (errorCallback_) {
      errorCallback_();
    }
  }

  // POLLRDHUP对等方关闭连接
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
    if (readCallback_) {
      readCallback_(receiveTime);
    }
  }

  if (revents_ & POLLOUT) {
    if (writeCallback_) {
      writeCallback_();
    }
  }

  eventHandling_ = false;
}

}  // namespace Muduo
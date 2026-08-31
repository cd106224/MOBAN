#include "EventLoop.h"

#include <signal.h>
#include <sys/eventfd.h>

#include <cstdlib>
#include <mutex>

#include "Channel.h"
#include "EPollPoller.h"
#include "TimerId.h"
#include "TimerQueue.h"
#include "base/current_thread.h"
#include "base/timestamp.h"
#include "callbacks.h"
#include "log/logging.h"

namespace {
__thread Muduo::EventLoop* t_loopInThisThread = nullptr;
const int KPollTimeMs = 10000;
int createEventfd() {
  int evfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evfd < 0) {
    LOG_ERROR("Failed in eventfd");
    exit(EXIT_FAILURE);
  }
  return evfd;
}

class IgnoreSigPipe {
 public:
  IgnoreSigPipe() { ::signal(SIGPIPE, SIG_IGN); }
};

IgnoreSigPipe initObj;

}  // namespace

namespace Muduo {

EventLoop* EventLoop::getEventLoopOfCurrentThread() {
  return t_loopInThisThread;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      started_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      threadId_(current_thread::id()),
      poller_(new EPollPoller(this)),
      timerQueue_(new TimerQueue(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      currentActiveChannel_(nullptr) {
  // fmt 新版本禁止直接格式化非 void 类型指针
  LOG_INFO("EventLoop created {} in thread {}", static_cast<const void*>(this),
           threadId_);
  // 如果当前线程已经创建了EventLoop,则终止
  if (t_loopInThisThread) {
    LOG_ERROR("Another EventLoop {} exists in this thread {}",
              static_cast<const void*>(t_loopInThisThread), threadId_);
    exit(EXIT_FAILURE);
  } else {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setReadCallback([this](const Timestamp&) { handleRead(); });
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

// 事件循环 该函数不能跨线程使用
// 只能在创建该对象的线程中使用
void EventLoop::loop() {
  assert(!looping_);
  assertInLoopThread();
  looping_ = true;
  // 首次进入不重置quit_,否则进入loop()前被调用的quit()(比如EventLoopThread析构)会被吞掉,导致循环永不退出
  // 重新进入时才清除上一次的quit请求,以支持同一EventLoop被多次loop()
  if (started_) {
    quit_ = false;
  }
  started_ = true;
  LOG_INFO("EventLoop {} start looping", static_cast<const void*>(this));
  while (!quit_) {
    activeChannels_.clear();
    pollReturnTime_ = poller_->poll(KPollTimeMs, &activeChannels_);
    eventHandling_ = true;
    for (auto& it : activeChannels_) {
      currentActiveChannel_ = it;
      it->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = nullptr;
    eventHandling_ = false;
    doPendingFunctors();
  }
  LOG_INFO("EventLoop {} stop looping", static_cast<const void*>(this));
  looping_ = false;
}

void EventLoop::assertInLoopThread() {
  if (!isInLoopThread()) {
    abortNotInLoopThread();
  }
}

bool EventLoop::isInLoopThread() { return threadId_ == current_thread::id(); }

void EventLoop::abortNotInLoopThread() {
  LOG_ERROR(
      "EventLoop::abortNotInLoopThread-EventLoop {} was created in "
      "threadId={},current threadId={}",
      static_cast<const void*>(this), threadId_, current_thread::id());
  exit(EXIT_FAILURE);
}

void EventLoop::updateChanel(Channel* channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_) {
    assert(currentActiveChannel_ == channel ||
           std::find(activeChannels_.begin(), activeChannels_.end(), channel) ==
               activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

Timestamp EventLoop::pollReturnTime() { return pollReturnTime_; }

// 该函数可以跨线程调用
void EventLoop::quit() {
  quit_ = true;
  if (!isInLoopThread()) {
    wakeup();
  }
}

TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb) {
  return timerQueue_->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, const TimerCallback& cb) {
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, cb);
}

TimerId EventLoop::runEvery(double interval, const TimerCallback& cb) {
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(cb, time, interval);
}

void EventLoop::cancel(TimerId timerId) { return timerQueue_->cancel(timerId); }

bool EventLoop::eventHandling() { return eventHandling_; }

void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_ERROR("EventLoop::wakeup writes {} bytes insted of 8", n);
    exit(EXIT_FAILURE);
  }
}

void EventLoop::handleRead() {
  uint64_t one = 1;
  auto n = ::read(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_ERROR("EventLoop::wakeup reads {} bytes insted of 8", n);
    exit(EXIT_FAILURE);
  }
}

void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;
  {
    std::lock_guard lk(mutex_);
    functors.swap(pendingFunctors_);
  }
  for (auto& v : functors) {
    v();  // functors有可能执行runInLoop
  }
  callingPendingFunctors_ = false;
}

void EventLoop::runInLoop(const Functor& cb) {
  if (isInLoopThread()) {
    cb();
  } else {
    queueInLoop(cb);  // 其他线程调用runInLoop,异步将cb加入队列
  }
}

void EventLoop::queueInLoop(const Functor& cb) {
  {
    std::lock_guard lk(mutex_);
    pendingFunctors_.emplace_back(cb);
  }
  // 调用queueInLoop的线程不是IO线程需要唤醒
  // 或者调用queuInLoop的线程是IO线程,并且此时正在调用pending functor,需要唤醒
  // 只有IO线程的事件回调中调用queuInLoop才不需要唤醒

  if (!isInLoopThread() || callingPendingFunctors_) {
    wakeup();
  }
}

}  // namespace Muduo
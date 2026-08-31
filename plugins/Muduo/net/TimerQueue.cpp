#include "TimerQueue.h"

#include <log/logging.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <ctime>

#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"
#include "base/timestamp.h"
#include "callbacks.h"

namespace {
int createTimerfd() {
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0) {
    LOG_ERROR("Failed in timerfd_create");
    exit(EXIT_FAILURE);
  }
  return timerfd;
}

timespec howMuchTimeFromNow(const Timestamp& when) {
  int64_t microseconds =
      when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
  if (microseconds < 100) {
    microseconds = 100;
  }
  timespec ts{};
  ts.tv_sec =
      static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000;
  return ts;
}

void readTimerfd(int timerfd, const Timestamp& now) {
  uint64_t howmany;
  ssize_t n = read(timerfd, &howmany, sizeof(howmany));
  LOG_INFO("TimerQueue::handleRead() {} at {}", howmany, now.toString());
  if (n != sizeof(howmany)) {
    LOG_ERROR("TimerQueue::handleRead() reads {} bytes instead of 8", n);
  }
}

void resetTimerfd(int timerfd, const Timestamp& expiration) {
  itimerspec newValue{};
  itimerspec oldValue{};
  newValue.it_value = howMuchTimeFromNow(expiration);
  int ret = timerfd_settime(timerfd, 0, &newValue, &oldValue);
  if (ret) {
    LOG_ERROR("timerfd_settime()");
  }
}

}  // namespace

namespace Muduo {
TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      callingExpiredTimers_(false) {
  timerfdChannel_.setReadCallback(
      [this](const Timestamp&) { this->handleRead(); });
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
  close(timerfd_);
  for (auto& v : timers_) {
    delete v.second;
  }
}

TimerId TimerQueue::addTimer(const TimerCallback& cb, Timestamp when,
                             double interval) {
  auto* timer = new Timer(cb, when, interval);
  // 可以跨线程使用
  loop_->runInLoop([this, timer] { addTimerInLoop(timer); });
  return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId) {
  // 可以跨线程使用
  loop_->runInLoop([this, timerId] { cancelInLoop(timerId); });
}

void TimerQueue::addTimerInLoop(Timer* timer) {
  loop_->assertInLoopThread();
  // 插入一个定时器,有可能会使得最早到期的定时器发生改变
  bool earliestChanged = insert(timer);
  if (earliestChanged) {
    // 重置定时器的超时时刻
    resetTimerfd(timerfd_, timer->expiration());
  }
}

void TimerQueue::cancelInLoop(TimerId timerId) {
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  ActiveTimer timer(timerId.timer_, timerId.sequenct_);
  auto it = activeTimers_.find(timer);
  // todo:为什么要用两个set？？？--陈硕说是防止timerId.m_timer指向已经被析构的对象
  if (it != activeTimers_.end()) {
    size_t n = timers_.erase({it->first->expiration(), it->first});
    assert(n == 1);
    (void)n;
    delete it->first;
    activeTimers_.erase(it);
  } else if (callingExpiredTimers_) {
    // 已经到期,并且正在调用回调函数的定时器
    cancelingTimers_.insert(timer);
  }
  assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead() {
  loop_->assertInLoopThread();
  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);  // 清除该事件 避免一直触发

  // 获取该时刻之前所有的定时器列表
  std::vector<Entry> expired = getExpired(now);

  // Qustion:m_cancelingTimers存在的意思是什么？？？
  //  我们可以知道  m_callingExpiredTimers
  //  变量除构造外出现三次,分别在handleRead和cancelInLoop中,
  //  我们可以考虑一个特殊情况,就是handleRead两个callingExpiredTimers_的中间的回调部分执行cancelInLoop,
  //  而此时timer中已经没有了这一项,如果这一项是可复用的,意味着在reset中又会加入,这样看来我们的回调并没有起作用,
  //  这时就引入了m_callingExpiredTimers
  callingExpiredTimers_ = true;
  cancelingTimers_.clear();

  for (auto& v : expired) {
    v.second->run();
  }
  callingExpiredTimers_ = false;
  // 不是一次性定时器,需要重启
  reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now) {
  assert(timers_.size() == activeTimers_.size());
  std::vector<Entry> expired;
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
  // 返回第一个未到期的Timer的迭代器
  // lower_bound的含义是返回第一个值>=sentry的元素的iterator
  // 即*end >= sentry，从而end->first > now
  auto end = timers_.lower_bound(sentry);
  assert(end == timers_.end() || now < end->first);
  // 将到期的定时器插入到expired中
  std::copy(timers_.begin(), end, std::back_inserter(expired));
  // 从timers_中移除到期的定时器
  timers_.erase(timers_.begin(), end);

  // 从activeTimers中移除到期的定时器
  for (auto& v : expired) {
    ActiveTimer timer(v.second, v.second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1);
    (void)n;
  }
  assert(timers_.size() == activeTimers_.size());
  return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now) {
  Timestamp nextExpire;
  for (const auto& it : expired) {
    ActiveTimer timer(it.second, it.second->sequence());
    // 如果是重复的定时器并且是未取消定时器,则重启该定时器
    if (it.second->repeat() &&
        cancelingTimers_.find(timer) == cancelingTimers_.end()) {
      it.second->restart(now);
      insert(it.second);
    } else {
      // 一次性定时器或者已被取消的定时器是不能重置的,因此删除该定时器
      delete it.second;
    }
  }
  if (!timers_.empty()) {
    // 获取最早到期的定时器超时时间
    nextExpire = timers_.begin()->second->expiration();
  }
  if (nextExpire.valid()) {
    // 重置定时器的超时时刻(timerfd_settime)
    resetTimerfd(timerfd_, nextExpire);
  }
}

bool TimerQueue::insert(Timer* timer) {
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  // 最早到期时间是否改变
  bool earliestChanged = false;
  Timestamp when = timer->expiration();
  auto it = timers_.begin();
  // 如果timers_为空或者when小于timers_中的最早到期时间
  if (it == timers_.end() || when < it->first) {
    earliestChanged = true;
  }
  {
    // 插入到timers_里面
    auto result = timers_.insert({when, timer});
    assert(result.second);
    (void)result;
  }
  {
    // 插入到activeTimers_里面
    auto result = activeTimers_.insert({timer, timer->sequence()});
    assert(result.second);
    (void)result;
  }
  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}

}  // namespace Muduo
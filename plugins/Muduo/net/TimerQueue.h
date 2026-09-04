#pragma once

#include <boost/core/noncopyable.hpp>
#include <set>

#include "Channel.h"
#include "base/timestamp.h"
#include "callbacks.h"

namespace Muduo {
class TimerId;
class EventLoop;
class Timer;

class TimerQueue : boost::noncopyable {
 public:
  explicit TimerQueue(EventLoop* loop);
  ~TimerQueue();

  // 增加定时器 一定是线程安全的，可以跨线程使用，通常情况下被其他线程调用
  TimerId addTimer(const TimerCallback& cb, Timestamp when, double interval);
  // 取消一个定时器
  void cancel(TimerId timerid);

  using Entry = std::pair<Timestamp, Timer*>;
  using TimerList = std::set<Entry>;
  using ActiveTimer = std::pair<Timer*, int64_t>;
  using ActiveTimerSet = std::set<ActiveTimer>;

 protected:
  // 以下成员函数只会在所属的io线程里面调用,所以不必要加锁
  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);
  void handleRead();
  // 返回超时的定时器列表
  std::vector<Entry> getExpired(Timestamp now);
  // 重置超时的定时器
  void reset(const std::vector<Entry>& expired, Timestamp now);
  bool insert(Timer* timer);

 protected:
  EventLoop* loop_;  // 所属EventLoop
  const int timerfd_;
  Channel timerfdChannel_;
  TimerList timers_;  // timers是按照到期时间排序的

  ActiveTimerSet activeTimers_;
  bool callingExpiredTimers_;       // atomic
  ActiveTimerSet cancelingTimers_;  // 保存的是被取消的定时器
};

}  // namespace Muduo
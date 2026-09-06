#pragma once

/**
 * @brief   EventLoop类的封装
 * @author  guogang
 */

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

#include "TimerId.h"
#include "TimerQueue.h"
#include "base/timestamp.h"
#include "callbacks.h"

namespace Muduo {
class Channel;
class EPollPoller;

class EventLoop : noncopyable {
 public:
  using Functor = std::function<void()>;
  EventLoop();
  ~EventLoop();
  void loop();
  void quit();
  Timestamp pollReturnTime();
  // 在I/O线程中执行某个回调函数,该函数可以跨线程调用
  void runInLoop(const Functor& cb);
  void queueInLoop(const Functor& cb);

  TimerId runAt(const Timestamp& time, const TimerCallback& cb);
  TimerId runAfter(double delay, const TimerCallback& cb);
  TimerId runEvery(double interval, const TimerCallback& cb);
  void cancel(TimerId timerId);
  void assertInLoopThread();
  bool isInLoopThread();
  bool eventHandling();
  // 唤醒epoller
  void wakeup();
  void updateChanel(Channel* channel);
  void removeChannel(Channel* channel);
  static EventLoop* getEventLoopOfCurrentThread();

 protected:
  void abortNotInLoopThread();
  void handleRead();
  void doPendingFunctors();

 protected:
  using ChannelList = std::vector<Channel*>;
  std::atomic<bool> looping_;
  std::atomic<bool> quit_;
  bool
      started_;  // loop()是否曾被进入过,用于区分首次进入(不重置quit_)和重新进入(重置)
  bool eventHandling_;
  std::atomic<bool> callingPendingFunctors_;
  const pid_t threadId_;
  Timestamp pollReturnTime_;
  std::unique_ptr<EPollPoller> poller_;
  std::unique_ptr<TimerQueue> timerQueue_;

  int wakeupFd_;                            // 用于eventfd
  std::unique_ptr<Channel> wakeupChannel_;  // 该通道会纳入Epoller来管理
  ChannelList activeChannels_;              // Poller返回的活动通道
  Channel* currentActiveChannel_;           // 当前正在处理的活动线程
  std::mutex mutex_;
  std::vector<Functor> pendingFunctors_;
};
}  // namespace Muduo
#pragma once

/**
 * @brief  EventLoop线程封装类
 * @author guogang
 */

#include <base/thread.h>
#include <utilities/noncopyable.h>

#include <functional>

namespace Muduo {
class EventLoop;

class EventLoopThread : noncopyable {
 public:
  using ThreadInitCallback = std::function<void(EventLoop*)>;
  explicit EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback());
  ~EventLoopThread();
  EventLoop* startLoop();

 protected:
  void threadFunc();

 protected:
  EventLoop* loop_;  // loop_指向一个EventLoop对象
  bool exiting_;
  std::mutex mutex_;
  std::condition_variable cond_;
  ThreadInitCallback callback_;  // 回调函数在EventLoop::Loop事件循环之前被调用
  // Thread在构造时就会启动线程,必须放到最后声明,保证mutex_/cond_/callback_先完成构造
  Thread thread_;
};

}  // namespace Muduo
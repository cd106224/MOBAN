#pragma once

#include <boost/core/noncopyable.hpp>
#include <functional>

namespace Muduo {
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : public boost::noncopyable {
 public:
  using ThreadInitCallback = std::function<void(EventLoop* loop)>;
  explicit EventLoopThreadPool(EventLoop* baseloop);
  ~EventLoopThreadPool();
  void setThreadNum(int numThread);
  void start(const ThreadInitCallback& cb = ThreadInitCallback());
  EventLoop* getNextLoop();

 protected:
  EventLoop* baseLoop_;  // 与Acceptor所属EvemtLoop相同
  bool started_;
  int numThread_;
  size_t next_;  // 新连接到来,选择的EventLoop对象下标

  std::vector<EventLoopThread*> threads_;
  std::vector<EventLoop*> loops_;
};

}  // namespace Muduo
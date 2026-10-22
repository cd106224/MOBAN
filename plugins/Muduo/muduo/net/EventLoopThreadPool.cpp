#include "EventLoopThreadPool.h"

#include <cassert>

#include "EventLoop.h"
#include "EventLoopThread.h"

namespace Muduo {
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseloop)
    : baseLoop_(baseloop), started_(false), numThread_(0), next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {
  // Don't delete loop, it's stack variable
  for (auto& v : threads_) {
    delete v;
    v = nullptr;
  }
}

void EventLoopThreadPool::setThreadNum(int numThread) {
  numThread_ = numThread;
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
  assert(!started_);
  baseLoop_->assertInLoopThread();
  started_ = true;
  for (int i = 0; i < numThread_; ++i) {
    auto* t = new EventLoopThread(cb);
    threads_.emplace_back(t);
    loops_.emplace_back(t->startLoop());
  }
  if (numThread_ == 0 && cb) {
    cb(baseLoop_);
  }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
  baseLoop_->assertInLoopThread();
  auto loop = baseLoop_;

  // 如果loops_为空,则loop执行baseLoop
  // 如果不为空,则round-robin(轮叫)的调度方式选择一个EventLoop
  if (!loops_.empty()) {
    loop = loops_[next_];
    ++next_;
    if (next_ >= loops_.size()) {
      next_ = 0;
    }
  }
  return loop;
}

}  // namespace Muduo
#include "EventLoopThread.h"

#include <mutex>

#include "EventLoop.h"

namespace Muduo {
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb)
    : loop_(nullptr),
      exiting_(false),
      mutex_(),
      cond_(),
      callback_(cb),
      thread_(Thread::Create("EventLoopThread", [this] { threadFunc(); })) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  EventLoop* loop = nullptr;
  {
    // Thread在构造函数里就启动了线程,这里要等threadFunc把loop_设置好,否则loop()还没开始就退出会空转
    std::unique_lock<std::mutex> lock(mutex_);
    while (loop_ == nullptr) {
      cond_.wait(lock);
    }
    loop = loop_;
  }

  // 以下是UB
  //{
  //   std::unique_lock<std::mutex> lock(mutex_);
  //   while (loop_ == nullptr) cond_.wait(lock);
  // }
  // loop_->quit();  // ← 锁已释放,这里读 loop_ 是未加锁读共享变量,UB

  loop->quit();
  thread_.join();
}

EventLoop* EventLoopThread::startLoop() {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    while (loop_ == nullptr) {
      cond_.wait(lock);
    }
  }
  return loop_;
}

void EventLoopThread::threadFunc() {
  EventLoop loop;
  if (callback_) {
    callback_(&loop);
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // loop_指针指向了一个栈上的对象,threadFunc函数退出之后,这个指针就失效了
    // threadFunc函数退出,就意味着线程退出了,EventLoopThread对象也就没有存在的价值了
    // 因而不会有什么大的问题
    loop_ = &loop;
    cond_.notify_one();
  }
  loop.loop();
}

}  // namespace Muduo
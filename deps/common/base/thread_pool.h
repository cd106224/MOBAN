#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

#include "thread.h"

class ThreadPool {
 public:
  using Task = std::function<void()>;
  explicit ThreadPool(const std::string &name = "ThreadPool");
  ~ThreadPool();
  void start(const int numThread);
  void stop();

  template <typename F>
  void run(F &&f) {
    {
      std::lock_guard<std::mutex> lk(mutex_);
      if (!running_.load(std::memory_order_relaxed)) {
        throw std::runtime_error("ThreadPool is not running");
      }
      queue_.emplace_back(std::forward<F>(f));
    }
    cond_.notify_one();
  }

 protected:
  void runInThread();

  std::mutex mutex_;
  std::condition_variable cond_;
  std::string name_;
  std::vector<std::shared_ptr<Thread>> threads_;
  std::deque<Task> queue_;
  std::atomic<bool> running_;
};
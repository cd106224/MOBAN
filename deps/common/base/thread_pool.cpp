#include "thread_pool.h"

#include "log/logging.h"

ThreadPool::ThreadPool(const std::string& name)
    : name_(name), running_(false) {}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::start(const int numThread) {
  if (running_.exchange(true)) {
    LOG_WARN("ThreadPool {} already started", name_);
    return;
  }

  threads_.reserve(numThread);
  for (auto i = 0; i < numThread; ++i) {
    std::string threadName = fmt::format("{}-{}", name_, i);
    threads_.emplace_back(std::make_shared<Thread>(
        Thread::Create(threadName, &ThreadPool::runInThread, this)));
  }
}

void ThreadPool::runInThread() {
  while (true) {
    Task task;
    {
      std::unique_lock<std::mutex> lk(mutex_);
      cond_.wait(lk, [this] {
        return !running_.load(std::memory_order_relaxed) || !queue_.empty();
      });

      if (!running_.load(std::memory_order_relaxed) && queue_.empty()) {
        return;  // 退出
      }

      task = std::move(queue_.front());
      queue_.pop_front();
    }
    task();  // 执行任务
  }
}

void ThreadPool::stop() {
  bool expected = true;
  if (!running_.compare_exchange_strong(expected, false,
                                        std::memory_order_relaxed)) {
    return;  // 已停止
  }
  {
    std::lock_guard<std::mutex> lk(mutex_);
    cond_.notify_all();
  }
  for (auto& t : threads_) {
    t->join();
  }
  threads_.clear();
}
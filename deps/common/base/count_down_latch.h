#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

class CountDownLatch {
 public:
  explicit CountDownLatch(std::ptrdiff_t count) : count_(count) {
    if (count < 0) throw std::invalid_argument("CountDownLatch count < 0");
  }

  CountDownLatch(const CountDownLatch&) = delete;
  CountDownLatch& operator=(const CountDownLatch&) = delete;

  void countDown() {
    std::lock_guard<std::mutex> lock(m_);
    if (count_ == 0) return;  // 已到达 0，直接退出
    if (--count_ == 0) cv_.notify_all();
  }

  void wait() {
    std::unique_lock<std::mutex> lock(m_);
    cv_.wait(lock,
             [this] { return count_.load(std::memory_order_acquire) == 0; });
  }

  template <typename Rep, typename Period>
  bool waitFor(const std::chrono::duration<Rep, Period>& timeout) {
    std::unique_lock<std::mutex> lock(m_);
    return cv_.wait_for(lock, timeout, [this] {
      return count_.load(std::memory_order_acquire) == 0;
    });
  }

  std::ptrdiff_t getCount() const noexcept {
    return count_.load(std::memory_order_acquire);
  }

 private:
  mutable std::mutex m_;
  std::condition_variable cv_;
  std::atomic<std::ptrdiff_t> count_;
};
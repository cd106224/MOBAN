#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class BlockingQueue {
 public:
  BlockingQueue() = default;
  ~BlockingQueue() = default;

  // 禁用拷贝，保留移动
  BlockingQueue(const BlockingQueue&) = delete;
  BlockingQueue& operator=(const BlockingQueue&) = delete;
  BlockingQueue(BlockingQueue&&) = default;
  BlockingQueue& operator=(BlockingQueue&&) = default;

  // 生产者接口
  void put(const T& value) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      q_.push(value);
    }
    cv_.notify_one();
  }

  void put(T&& value) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      q_.push(std::move(value));
    }
    cv_.notify_one();
  }

  // 消费者接口
  T take() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this] { return !q_.empty() || done_; });

    if (done_ && q_.empty()) {
      throw std::runtime_error("queue is closed");
    }

    T value = std::move(q_.front());
    q_.pop();
    return value;
  }

  // 非阻塞 try_take
  std::optional<T> try_take() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (q_.empty() || done_) return std::nullopt;

    T value = std::move(q_.front());
    q_.pop();
    return value;
  }

  // 关闭队列：让阻塞在 take() 上的线程优雅退出
  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      done_ = true;
    }
    cv_.notify_all();
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return q_.size();
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return q_.empty();
  }

 private:
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::queue<T> q_;
  bool done_{false};
};
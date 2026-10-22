#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

namespace common::base {

template <typename T>
class BoundedBlockingQueue {
 public:
  explicit BoundedBlockingQueue(size_t max_size) : max_size_(max_size) {
    buffer_.resize(max_size_);
  }

  ~BoundedBlockingQueue() { stop(); }

  BoundedBlockingQueue(const BoundedBlockingQueue&) = delete;
  BoundedBlockingQueue& operator=(const BoundedBlockingQueue&) = delete;

  void stop() {
    std::lock_guard<std::mutex> lock(mu_);
    stopped_ = true;
    cv_empty_.notify_all();
    cv_full_.notify_all();
  }

  void push(const T& value) {
    std::unique_lock<std::mutex> lock(mu_);
    cv_full_.wait(lock, [this] { return !is_full_locked() || stopped_; });
    if (stopped_) return;
    emplace_internal(value);
  }

  bool try_push(const T& value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (is_full_locked() || stopped_) return false;
    emplace_internal(value);
    return true;
  }

  std::optional<T> pop() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_empty_.wait(lock, [this] { return !is_empty_locked() || stopped_; });
    if (stopped_) return std::nullopt;
    return pop_internal();
  }

  template <typename Rep, typename Period>
  std::optional<T> try_pop_for(
      const std::chrono::duration<Rep, Period>& timeout) {
    std::unique_lock<std::mutex> lock(mu_);
    if (!cv_empty_.wait_for(
            lock, timeout, [this] { return !is_empty_locked() || stopped_; })) {
      return std::nullopt;
    }
    if (stopped_) return std::nullopt;
    return pop_internal();
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return size_;
  }

  size_t max_size() const { return max_size_; }

 private:
  bool is_empty_locked() const { return size_ == 0; }
  bool is_full_locked() const { return size_ == max_size_; }

  template <typename U>
  void emplace_internal(U&& value) {
    buffer_[back_] = std::forward<U>(value);
    back_ = (back_ + 1) % max_size_;
    ++size_;
    cv_empty_.notify_one();
  }

  T pop_internal() {
    T value = std::move(buffer_[front_]);
    front_ = (front_ + 1) % max_size_;
    --size_;
    cv_full_.notify_one();
    return value;
  }

  mutable std::mutex mu_;
  std::condition_variable cv_empty_;
  std::condition_variable cv_full_;

  std::vector<T> buffer_;
  const size_t max_size_;
  size_t size_ = 0;
  size_t front_ = 0;
  size_t back_ = 0;
  bool stopped_ = false;
};

}  // namespace common::base
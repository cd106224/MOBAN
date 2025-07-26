#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

template <typename T>
class BoundedBlockingQueue {
 public:
  explicit BoundedBlockingQueue(size_t max_size) : max_size_(max_size) {
    buffer_.resize(max_size_);
  }

  ~BoundedBlockingQueue() { Stop(); }

  BoundedBlockingQueue(const BoundedBlockingQueue&) = delete;
  BoundedBlockingQueue& operator=(const BoundedBlockingQueue&) = delete;

  void Stop() {
    std::lock_guard<std::mutex> lock(mu_);
    stopped_ = true;
    cv_empty_.notify_all();
    cv_full_.notify_all();
  }

  void Push(const T& value) {
    std::unique_lock<std::mutex> lock(mu_);
    cv_full_.wait(lock, [this] { return !IsFullLocked() || stopped_; });
    if (stopped_) return;
    EmplaceInternal(value);
  }

  bool TryPush(const T& value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (IsFullLocked() || stopped_) return false;
    EmplaceInternal(value);
    return true;
  }

  std::optional<T> Pop() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_empty_.wait(lock, [this] { return !IsEmptyLocked() || stopped_; });
    if (stopped_) return std::nullopt;
    return PopInternal();
  }

  template <typename Rep, typename Period>
  std::optional<T> TryPopFor(
      const std::chrono::duration<Rep, Period>& timeout) {
    std::unique_lock<std::mutex> lock(mu_);
    if (!cv_empty_.wait_for(lock, timeout,
                            [this] { return !IsEmptyLocked() || stopped_; })) {
      return std::nullopt;
    }
    if (stopped_) return std::nullopt;
    return PopInternal();
  }

  size_t Size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return size_;
  }

  size_t MaxSize() const { return max_size_; }

 private:
  bool IsEmptyLocked() const { return size_ == 0; }
  bool IsFullLocked() const { return size_ == max_size_; }

  template <typename U>
  void EmplaceInternal(U&& value) {
    buffer_[back_] = std::forward<U>(value);
    back_ = (back_ + 1) % max_size_;
    ++size_;
    cv_empty_.notify_one();
  }

  T PopInternal() {
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
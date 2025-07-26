#pragma once
#include <memory>

template <typename T>
class ThreadLocal {
 public:
  ThreadLocal() = default;
  ThreadLocal(const ThreadLocal&) = delete;
  ThreadLocal& operator=(const ThreadLocal&) = delete;

  T& operator*() { return value(); }
  T* operator->() { return &value(); }
  const T& operator*() const { return value(); }
  const T* operator->() const { return &value(); }

  T& value() {
    static thread_local std::unique_ptr<T> ptr;
    if (!ptr) ptr = std::make_unique<T>();
    return *ptr;
  }
};
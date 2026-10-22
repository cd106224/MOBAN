#pragma once

#include <sys/types.h>

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>

namespace common::base {

class Thread {
 public:
  template <typename Func, typename... Args>
  static Thread Create(const std::string& name, Func&& func, Args&&... args) {
    return start_thread(name, std::bind(func, std::forward<Args>(args)...));
  }
  Thread(const Thread&) = delete;
  Thread(Thread&&) = default;
  Thread& operator=(const Thread&) = delete;
  Thread& operator=(Thread&&) = delete;
  void join() {
    if (thread_->joinable()) {
      thread_->join();
    }
  }
  void detach() const { thread_->detach(); }
  const std::string& name() const { return name_; }
  int64_t tid() const { return tid_; }
  void set_tid(const pid_t tid) { tid_ = tid; }

 private:
  explicit Thread(const std::string& name);
  static Thread start_thread(const std::string& name,
                             const std::function<void()>& func);
  static void monitor_thread(const std::function<void()>& func,
                             std::promise<pid_t>* tid,
                             const std::string& thread_name);
  std::unique_ptr<std::thread> thread_;
  const std::string name_{};
  pid_t tid_{-2};
};

}  // namespace common::base
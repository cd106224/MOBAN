#pragma once

#include <functional>
#include <string>
#include <thread>

class Thread {
 public:
  template <typename Func, typename... Args>
  static Thread Create(const std::string& name, Func&& func, Args&&... args) {
    return StartThread(name, std::bind(func, std::forward<Args>(args)...));
  }
  Thread(const Thread&) = delete;
  Thread(Thread&&) = default;
  Thread& operator=(const Thread&) = delete;
  Thread& operator=(Thread&&) = delete;
  void join() const {
    if (thread_->joinable()) {
      thread_->join();
    }
  }
  void detach() const { thread_->detach(); }
  const std::string& name() const { return name_; }
  int64_t tid() const { return tid_; }

 private:
  explicit Thread(const std::string& name);
  static Thread StartThread(const std::string& name,
                            const std::function<void()>& func);
  static void MonitorThread(const Thread* thread, const std::function<void()>& func);
  std::unique_ptr<std::thread> thread_;
  const std::string name_;
  pid_t tid_{-2};
};
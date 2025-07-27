#pragma once

#include <functional>
#include <future>
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
  void set_tid(const pid_t tid) { tid_ = tid; }

 private:
  explicit Thread(const std::string& name);
  static Thread StartThread(const std::string& name,
                            const std::function<void()>& func);
  static void MonitorThread(const std::function<void()>& func,
                            std::promise<pid_t>* tid,
                            const std::string& thread_name);
  std::unique_ptr<std::thread> thread_;
  const std::string name_{};
  pid_t tid_{-2};
};
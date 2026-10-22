#include "thread.h"

#include <log/logging.h>

#include "current_thread.h"

namespace common::base {

Thread::Thread(const std::string& name) : name_(name) {}

Thread Thread::start_thread(const std::string& name,
                            const std::function<void()>& func) {
  Thread t(name);
  std::promise<pid_t> tid;
  t.thread_.reset(new std::thread(&Thread::monitor_thread, func, &tid, name));
  t.tid_ = tid.get_future().get();
  return t;
}

void Thread::monitor_thread(const std::function<void()>& func,
                            std::promise<pid_t>* tid,
                            const std::string& thread_name) {
  tid->set_value(current_thread::id());
  current_thread::set_thread_name(thread_name);
  try {
    func();
    current_thread::set_thread_name("finished");
  } catch (...) {
    current_thread::set_thread_name("crashed");
    LOG_ERROR("unknown exception caught in Thread:{}", thread_name);
    // throw;  // rethrow
  }
}

}  // namespace common::base
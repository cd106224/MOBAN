#include "thread.h"

#include <log/logging.h>

#include "current_thread.h"

Thread::Thread(const std::string& name) : name_(name) {
  tid_ = current_thread::id();
  current_thread::set_thread_name(name.c_str());
}

Thread Thread::StartThread(const std::string& name,
                           const std::function<void()>& func) {
  Thread t(name);
  t.thread_.reset(new std::thread(&Thread::MonitorThread, &t, func));
  return t;
}

void MonitorThread(const Thread* thread, const std::function<void()>& func) {
  try {
    func();
    current_thread::set_thread_name("finished");
  } catch (...) {
    current_thread::set_thread_name("crashed");
    // TODO: 改成spdlog
    // fprintf(stderr, "unknown exception caught in Thread %s\n",
    // m_name.c_str());
    LOG_ERROR("unknown exception caught in Thread:{}", thread->name());
    throw;  // rethrow
  }
}
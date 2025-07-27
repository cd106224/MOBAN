#include "current_thread.h"

#include <sys/syscall.h>

#include <cstdio>

#include "thread_local.h"
#include "unistd.h"

namespace current_thread {
namespace detail {
inline pid_t gettid() noexcept {
  return static_cast<pid_t>(::syscall(SYS_gettid));
}
struct Cache {
  pid_t tid{};
  char tidStr[32]{};
  std::string threadName = "unKnown";
  Cache() {
    tid = gettid();
    std::snprintf(tidStr, sizeof(tidStr), "%5d ", tid);
  }
};
inline ThreadLocal<Cache>& cache() noexcept {
  static ThreadLocal<Cache> cache;
  return cache;
}
}  // namespace detail
pid_t id() noexcept { return detail::cache()->tid; }
bool isMain() noexcept { return id() == detail::gettid(); }
void set_thread_name(const std::string& name) noexcept {
  detail::cache()->threadName = name;
}
std::string thread_name() noexcept { return detail::cache()->threadName; }
}  // namespace current_thread
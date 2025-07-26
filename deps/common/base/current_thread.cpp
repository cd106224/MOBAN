#include "current_thread.h"

#include <sys/syscall.h>

#include "thread_local.h"

namespace current_thread {
namespace detail {
inline pid_t gettid() noexcept {
  return static_cast<pid_t>(::syscall(SYS_gettid));
}
struct Cache {
  pid_t tid{};
  char tidStr[32]{};
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
inline pid_t id() noexcept { return detail::cache()->tid; }
inline bool isMain() noexcept { return id() == detail::gettid(); }
}  // namespace current_thread
#include "current_thread.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/syscall.h>

#include "unistd.h"
#endif

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace current_thread {
namespace detail {
inline int64_t gettid() noexcept {
#ifdef _WIN32
  return GetCurrentThreadId();
#else
  return static_cast<pid_t>(::syscall(SYS_gettid));
#endif
}
struct Cache {
  int64_t tid{};
  char tidStr[32]{};
  char threadName[32] = "unKnown";
  Cache() {
    tid = gettid();
    std::snprintf(tidStr, sizeof(tidStr), "%6d" PRId64, tid);
  }
};
inline Cache& cache() noexcept {
  static thread_local Cache cache;
  return cache;
}
}  // namespace detail
int64_t id() noexcept { return detail::cache().tid; }
bool isMain() noexcept { return id() == detail::gettid(); }
void set_thread_name(const std::string& name) noexcept {
  constexpr std::size_t kMax = sizeof(detail::cache().threadName);
  auto* buf = detail::cache().threadName;  // 只取一次指针
  std::strncpy(buf, name.c_str(), kMax - 1);
  buf[kMax - 1] = '\0';
}
std::string thread_name() noexcept { return detail::cache().threadName; }
}  // namespace current_thread
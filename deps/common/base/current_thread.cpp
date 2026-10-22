#include "current_thread.h"

#ifdef _WIN32
#include <windows.h>

#include <atomic>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace common::base {
namespace detail {
inline int64_t get_tid() noexcept {
#ifdef _WIN32
  return static_cast<int64_t>(::GetCurrentThreadId());
#else
  return static_cast<pid_t>(::syscall(SYS_gettid));
#endif
}
struct Cache {
  int64_t tid{};
  char tidStr[32]{};
  char threadName[32] = "unKnown";
  Cache() {
    tid = get_tid();
    std::snprintf(tidStr, sizeof(tidStr), "%6" PRId64, tid);
  }
};
inline Cache& cache() noexcept {
  static thread_local Cache cache;
  return cache;
}

#ifdef _WIN32
// 命名空间作用域对象的动态初始化发生在 main() 之前、且由主线程执行，
// 所以这里捕获到的就是主线程的 tid，作为 is_main() 的比较基准。
struct MainThreadId {
  std::atomic<int64_t> value{0};
  MainThreadId() { value.store(get_tid(), std::memory_order_relaxed); }
};
const MainThreadId kMainThreadId;
#endif
}  // namespace detail

namespace current_thread {

int64_t id() noexcept { return detail::cache().tid; }

// Linux 上主线程的 tid 就是进程 pid，可以直接比较；
// Windows 没有"主线程 tid == 进程 id"的保证，只能用启动时捕获的主线程 tid。
bool is_main() noexcept {
#ifdef _WIN32
  return id() == detail::kMainThreadId.value.load(std::memory_order_relaxed);
#else
  return id() == ::getpid();
#endif
}
void set_thread_name(const std::string& name) noexcept {
  constexpr std::size_t kMax = sizeof(detail::cache().threadName);
  auto* buf = detail::cache().threadName;  // 只取一次指针
  std::strncpy(buf, name.c_str(), kMax - 1);
  buf[kMax - 1] = '\0';
}
std::string thread_name() noexcept { return detail::cache().threadName; }

}  // namespace current_thread
}  // namespace common::base

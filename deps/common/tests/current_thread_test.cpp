#include "base/current_thread.h"

#include <gtest/gtest.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/syscall.h>
#endif

#include <cstdint>
#include <string>
#include <thread>

using common::base::current_thread::id;
using common::base::current_thread::is_main;
using common::base::current_thread::set_thread_name;
using common::base::current_thread::thread_name;

namespace {
// 不走缓存的实时 tid，用来验证 id() 缓存的是本线程的 tid
int64_t raw_tid() {
#ifdef _WIN32
  return static_cast<int64_t>(::GetCurrentThreadId());
#else
  return static_cast<int64_t>(::syscall(SYS_gettid));
#endif
}
}  // namespace

TEST(CurrentThreadTest, IdIsCurrentThreadTid) { EXPECT_EQ(id(), raw_tid()); }

TEST(CurrentThreadTest, IsMainTrueInMainThread) { EXPECT_TRUE(is_main()); }

TEST(CurrentThreadTest, IsMainFalseInChildThread) {
  bool child_is_main = true;
  std::thread t([&child_is_main] { child_is_main = is_main(); });
  t.join();
  EXPECT_FALSE(child_is_main);
}

TEST(CurrentThreadTest, ThreadNameRoundTrip) {
  set_thread_name("io-worker");
  EXPECT_EQ(thread_name(), "io-worker");
}

TEST(CurrentThreadTest, ThreadNameTruncatedToBuffer) {
  set_thread_name(std::string(64, 'x'));
  EXPECT_EQ(thread_name().size(), 31u);
}

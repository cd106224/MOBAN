#include "base/count_down_latch.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

namespace {

TEST(CountDownLatchTest, SingleThread) {
  CountDownLatch latch(2);
  EXPECT_EQ(latch.getCount(), 2);

  latch.countDown();
  EXPECT_EQ(latch.getCount(), 1);

  latch.countDown();
  EXPECT_EQ(latch.getCount(), 0);
  EXPECT_TRUE(latch.waitFor(std::chrono::milliseconds(0)));  // 立即返回
}

TEST(CountDownLatchTest, WaitUntilZero) {
  CountDownLatch latch(1);

  std::thread t([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    latch.countDown();
  });

  latch.wait();  // 阻塞直到 count 为 0
  EXPECT_EQ(latch.getCount(), 0);
  t.join();
}

TEST(CountDownLatchTest, WaitForTimeout) {
  CountDownLatch latch(1);

  EXPECT_FALSE(latch.waitFor(std::chrono::milliseconds(5)));  // 超时
  EXPECT_EQ(latch.getCount(), 1);

  latch.countDown();
  EXPECT_TRUE(latch.waitFor(std::chrono::milliseconds(0)));  // 立即返回
}

TEST(CountDownLatchTest, MultipleWaiters) {
  constexpr int kThreads = 10;
  CountDownLatch latch(1);

  std::vector<std::thread> threads;
  std::vector<bool> results(kThreads, false);

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&, i] {
      latch.wait();
      results[i] = true;  // 被唤醒后置位
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  latch.countDown();

  for (auto& t : threads) t.join();
  for (bool r : results) EXPECT_TRUE(r);  // 所有线程都应通过
}

TEST(CountDownLatchTest, CountDownMoreThanInitial) {
  CountDownLatch latch(3);
  latch.countDown();
  latch.countDown();
  latch.countDown();
  latch.countDown();  // 超出无影响
  EXPECT_EQ(latch.getCount(), 0);
  EXPECT_TRUE(latch.waitFor(std::chrono::milliseconds(0)));
}

}  // namespace
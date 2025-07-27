#include "base/thread_pool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>

class ThreadPoolTest : public ::testing::Test {
 protected:
  void SetUp() override { pool_ = std::make_unique<ThreadPool>("TestPool"); }

  void TearDown() override { pool_->stop(); }

  std::unique_ptr<ThreadPool> pool_;
};

/* 1. 基本启动/停止 */
TEST_F(ThreadPoolTest, StartStop) {
  EXPECT_NO_THROW(pool_->start(4));
  EXPECT_NO_THROW(pool_->stop());
  EXPECT_NO_THROW(pool_->start(2));  // 可以再次启动
}

/* 2. 顺序执行：任务按 FIFO 顺序完成 */
TEST_F(ThreadPoolTest, TasksExecuteInOrder) {
  pool_->start(1);  // 单线程保证顺序
  std::vector<int> result;
  for (int i = 0; i < 10; ++i) {
    pool_->run([i, &result] { result.push_back(i); });
  }
  pool_->stop();  // 等待全部完成
  EXPECT_EQ(result.size(), 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(result[i], i);
  }
}

/* 3. 并发提交：多线程同时 submit，无数据竞争 */
TEST_F(ThreadPoolTest, ConcurrentSubmit) {
  pool_->start(4);
  std::atomic<int> counter{0};
  constexpr int kTasks = 1000;

  std::vector<std::thread> producers;
  for (int t = 0; t < 4; ++t) {
    producers.emplace_back([&] {
      for (int i = 0; i < kTasks / 4; ++i) {
        pool_->run([&] { counter.fetch_add(1, std::memory_order_relaxed); });
      }
    });
  }
  for (auto& th : producers) th.join();
  pool_->stop();
  EXPECT_EQ(counter.load(), kTasks);
}

/* 4. 任务抛异常不会导致程序终止 */
TEST_F(ThreadPoolTest, ExceptionInTask) {
  pool_->start(2);
  std::atomic<int> ok_tasks{0};

  // 第一个任务抛异常
  pool_->run([] { throw std::runtime_error("oops"); });
  // 第二个任务正常
  pool_->run([&] { ok_tasks++; });

  pool_->stop();
  EXPECT_EQ(ok_tasks.load(), 1);
}

/* 5. stop 后不再执行任务 */
TEST_F(ThreadPoolTest, TasksAfterStopAreDiscarded) {
  pool_->start(4);
  std::atomic<int> run_count{0};

  // 提交一个慢任务
  pool_->run([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    run_count++;
  });

  pool_->stop();  // 立即调用

  // 再提交任务应抛异常
  EXPECT_THROW(pool_->run([] {}), std::runtime_error);
  EXPECT_EQ(run_count.load(), 1);  // 只有慢任务被执行
}
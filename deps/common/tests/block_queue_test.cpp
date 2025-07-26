#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "base/blocking_queue.h"

using namespace std::chrono_literals;

class BlockingQueueTest : public ::testing::Test {
 protected:
  BlockingQueue<int> q;
};

/* 1. 单线程顺序读写 */
TEST_F(BlockingQueueTest, SequentialPutTake) {
  q.put(1);
  q.put(2);
  EXPECT_EQ(q.take(), 1);
  EXPECT_EQ(q.take(), 2);
  EXPECT_TRUE(q.empty());
}

/* 2. 关闭后抛异常 */
TEST_F(BlockingQueueTest, ShutdownThrows) {
  q.put(10);
  q.shutdown();
  EXPECT_EQ(q.take(), 10);
  EXPECT_THROW(q.take(), std::runtime_error);
}

/* 3. try_take 行为 */
TEST_F(BlockingQueueTest, TryTake) {
  EXPECT_FALSE(q.try_take().has_value());
  q.put(42);
  auto v = q.try_take();
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, 42);
  EXPECT_FALSE(q.try_take().has_value());
}

/* 4. 并发多生产者多消费者 */
TEST_F(BlockingQueueTest, MultiProducerMultiConsumer) {
  constexpr int kItemsPerThread = 10'000;
  constexpr int kThreads = 4;

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  std::atomic<int> produced{0};
  std::atomic<int> consumed{0};

  /* 生产者 */
  for (int i = 0; i < kThreads; ++i) {
    producers.emplace_back([&] {
      for (int j = 0; j < kItemsPerThread; ++j) {
        q.put(j);
        ++produced;
      }
    });
  }

  /* 消费者 */
  for (int i = 0; i < kThreads; ++i) {
    consumers.emplace_back([&] {
      try {
        for (;;) {
          q.take();
          ++consumed;
        }
      } catch (const std::runtime_error&) {
        // queue closed, exit gracefully
      }
    });
  }

  /* 等待生产完毕 */
  for (auto& t : producers) t.join();

  /* 等队列完全为空（消费者把剩余数据全部取完） */
  while (!q.empty()) {
    std::this_thread::sleep_for(1ms);  // 让出 CPU
  }

  /* 关闭队列，让消费者全部退出 */
  q.shutdown();

  for (auto& t : consumers) t.join();

  EXPECT_EQ(produced.load(), kThreads * kItemsPerThread);
  EXPECT_EQ(consumed.load(), produced.load());
}

/* 5. 队列统计接口 */
TEST_F(BlockingQueueTest, SizeAndEmpty) {
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0u);

  q.put(1);
  q.put(2);
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(q.size(), 2u);

  q.take();
  EXPECT_EQ(q.size(), 1u);

  q.take();
  EXPECT_TRUE(q.empty());
}
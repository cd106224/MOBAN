#include "base/bounded_blocking_queue.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

class BoundedBlockingQueueTest : public ::testing::Test {
 protected:
  BoundedBlockingQueue<int> queue_{2};
};

//----------------------------------------
// 基本功能
//----------------------------------------

TEST_F(BoundedBlockingQueueTest, PushPopWorks) {
  EXPECT_TRUE(queue_.TryPush(1));
  EXPECT_TRUE(queue_.TryPush(2));
  EXPECT_EQ(queue_.Size(), 2u);

  EXPECT_EQ(queue_.Pop(), 1);
  EXPECT_EQ(queue_.Pop(), 2);
  EXPECT_EQ(queue_.Size(), 0u);
}

TEST_F(BoundedBlockingQueueTest, TryPushFailsWhenFull) {
  EXPECT_TRUE(queue_.TryPush(1));
  EXPECT_TRUE(queue_.TryPush(2));
  EXPECT_FALSE(queue_.TryPush(3));  // full
}

TEST_F(BoundedBlockingQueueTest, TryPopForTimesOut) {
  EXPECT_EQ(queue_.TryPopFor(std::chrono::milliseconds(10)), std::nullopt);
}

//----------------------------------------
// 并发测试
//----------------------------------------

TEST_F(BoundedBlockingQueueTest, ConcurrentProducerConsumer) {
  constexpr int kItems = 1'000;

  std::thread producer([this] {
    for (int i = 0; i < kItems; ++i) {
      queue_.Push(i);
    }
  });

  std::thread consumer([this] {
    int sum = 0;
    for (int i = 0; i < kItems; ++i) {
      sum += queue_.Pop().value_or(-1);
    }
    EXPECT_EQ(sum, (kItems - 1) * kItems / 2);  // 0..kItems-1
  });

  producer.join();
  consumer.join();
}

//----------------------------------------
// Stop 测试
//----------------------------------------
TEST_F(BoundedBlockingQueueTest, StopUnblocksBlockedPop) {
  constexpr int kWaiters = 3;

  // 1. 原子计数器：记录已进入 Pop() 的 waiter 数量
  std::atomic<int> entered{0};
  std::atomic<bool> may_block{false};  // 主线程控制“可以阻塞”

  std::vector<std::thread> waiters;
  std::vector<std::optional<int>> results(kWaiters);

  // 2. 启动 waiter
  for (int i = 0; i < kWaiters; ++i) {
    waiters.emplace_back([this, i, &entered, &may_block, &results] {
      // 通知主线程：我已经到了
      ++entered;
      // 自旋等待主线程“放行”
      while (!may_block.load(std::memory_order_acquire))
        std::this_thread::yield();

      // 此时队列空，应该阻塞
      results[i] = queue_.Pop();
    });
  }

  // 3. 主线程确认所有 waiter 已到达
  while (entered.load() != kWaiters) std::this_thread::yield();
  std::this_thread::sleep_for(
      std::chrono::milliseconds(10));  // 再让出 CPU，确保阻塞

  // 4. 放行 waiter，让它们真正阻塞在 Pop()
  may_block.store(true, std::memory_order_release);

  // 5. 此刻所有 waiter 阻塞；调用 Stop() 必须唤醒它们
  queue_.Stop();

  for (auto& t : waiters) t.join();

  // 6. 验证结果
  for (const auto& r : results) {
    EXPECT_FALSE(r.has_value());  // 全部被 Stop 唤醒，返回 nullopt
  }
}
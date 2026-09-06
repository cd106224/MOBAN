#pragma once

/**
 * @brief   固定大小线程池,用于执行可能阻塞的磁盘 IO
 *
 * 为什么需要它:
 *   普通文件 fd 的 O_NONBLOCK 无效,epoll 对其永远报告可读。
 *   在 IO 线程里对磁盘文件做 read/stat,一旦 cache 未命中
 *   (冷读、HDD、NFS),系统调用会同步等待数毫秒甚至更久,
 *   期间该 loop 上所有连接全部停摆。
 *   本池把这些系统调用挪出去,完成后由调用方排队回 IO 线程。
 *
 * 使用约束:
 *   任务在别的线程执行,任务内不得触碰会话状态;
 *   结果必须通过 EventLoop::queueInLoop 交回 IO 线程处理。
 *   submit 绝不能阻塞 IO 线程:队列满时 trySubmit 直接拒绝,
 *   调用方(会话)收到 false 后以错误应答终止本次传输。
 *
 * @author  guogang
 */

#include <base/blocking_queue.h>

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace ftp {

class ThreadPool {
 public:
  using Task = std::function<void()>;

  // 有界队列容量:磁盘并发 = 线程数,队列只需吸收突发,
  // 真积压到 kMaxPending 说明磁盘已严重过载,拒绝比堆积更健康
  static constexpr size_t kMaxPending = 256;

  explicit ThreadPool(int numThreads) {
    for (int i = 0; i < numThreads; ++i) {
      workers_.emplace_back([this] { run(); });
    }
  }

  ~ThreadPool() {
    queue_.shutdown();
    for (auto& t : workers_) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  // 非阻塞提交:队列满返回 false,绝不等待(调用方在 IO 线程)
  bool trySubmit(Task task) {
    if (pending_ >= kMaxPending) {
      return false;
    }
    ++pending_;
    queue_.put([this, task = std::move(task)] {
      task();
      --pending_;
    });
    return true;
  }

 private:
  void run() {
    while (true) {
      try {
        Task task = queue_.take();
        task();
      } catch (const std::runtime_error&) {
        return;  // 队列关闭,线程退出
      }
    }
  }

  BlockingQueue<Task> queue_;
  std::atomic<size_t> pending_{0};
  std::vector<std::thread> workers_;
};

}  // namespace ftp

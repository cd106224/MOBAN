# MOBAN Common Base

`deps/common/base/` 是进程级的**基础构件**：线程、线程池、时间戳、并发队列、线程局部信息、
符号名反混淆。

按 `deps/common/README.md` 的目录划分，这里的模块"不依赖其他模块（`log/` 除外）"，
对外只暴露标准 C++ 类型。

---

## 1. 组件速查

| 组件 | 头文件 | 一句话 | 命名空间 |
| --- | --- | --- | --- |
| `Thread` | `base/thread.h` | 带名字和 tid 的线程封装 | `common::base` |
| `ThreadPool` | `base/thread_pool.h` | 固定线程数 + 任务队列 | `common::base` |
| `current_thread` | `base/current_thread.h` | 当前线程的 tid / 名字（线程局部） | `common::base::current_thread` |
| `BlockingQueue` | `base/blocking_queue.h` | 无界阻塞队列 | `common::base` |
| `BoundedBlockingQueue` | `base/bounded_blocking_queue.h` | 定长环形阻塞队列 | `common::base` |
| `CountDownLatch` | `base/count_down_latch.h` | 计数门闩 | `common::base` |
| `Timestamp` | `base/timestamp.h` | 微秒精度时间点 | `common::base` |
| `demangle` | `base/demangle.h` | C++ 符号名反混淆 | `common::base` |

**命名空间现状**：本目录所有对外类型都在 `common::base` 下（`current_thread` 作为其中的
子命名空间 `common::base::current_thread`），实现细节统一放在 `common::base::detail`。
调用方需显式限定，不提供全局别名。

---

## 2. `Thread`

带名字和 tid 的线程封装。内部起一个 `std::thread`，真正的入口是 `monitor_thread`。

```cpp
common::base::Thread t = common::base::Thread::Create("worker", []{ do_work(); });
t.join();                    // 或 t.detach()
const std::string& name = t.name();
int64_t tid = t.tid();
```

### 语义

- `Create(name, func, args...)` 是**静态工厂**，用 `std::bind` 绑定参数，返回 `Thread`。
- 线程真正的入口是 `monitor_thread`，它做三件事：
  1. 把 `current_thread::id()` 通过 `std::promise` 回传给创建者（所以 `Create` 返回前
     `tid()` 一定已经有效）
  2. `current_thread::set_thread_name(name)` 设置线程名
  3. 执行用户函数。正常结束把线程名改成 `"finished"`，抛异常则改成 `"crashed"` 并
     `LOG_ERROR` 记一条
- **线程体里抛出的异常会被吞掉**（`catch (...)` 只记日志，不重抛）。调用方无法感知
  线程内失败，只能靠日志。

### 注意事项

- **调用方必须 `join()` 或 `detach()`**。`Thread` 内部持有 `std::unique_ptr<std::thread>`，
  析构时若线程仍 joinable，`std::thread` 的析构函数会 `std::terminate()`。
- **线程体抛出的异常不会传播给创建者**（见上），这一点在 `ThreadPool` 上尤其要注意 ——
  任务里的异常同样只会被记日志。
- `tid()` 返回 `int64_t`，而内部成员是 `pid_t`，类型不完全一致。
- 拷贝被禁用，移动构造保留，**移动赋值被禁用**（`operator=(Thread&&) = delete`）。
  这不影响 `std::vector<Thread>::push_back`（vector 扩容走移动构造），但不能对
  已有元素做移动赋值。
- `set_tid()` 存在但 `start_thread` 内部直接写成员，实际上没人用它。

---

## 3. `ThreadPool`

固定数量的工作线程 + 一个任务队列。

```cpp
common::base::ThreadPool pool("io");
pool.start(4);                 // 起 4 个工作线程

pool.run([]{ task_a(); });     // 投递任务；未运行时抛 std::runtime_error
pool.stop();                   // 停止并 join 全部线程
```

### 语义

- `start(numThread)`：重复调用会被拒绝（`LOG_WARN` 后直接返回），但**停止后可以重启**。
- `stop()`：置 `running_ = false` → `notify_all` → `join` 所有线程。
  **队列里剩余的任务会被排空执行完**，不是直接丢弃。
- `run(f)`：在未运行时抛 `std::runtime_error("ThreadPool is not running")`。
  任务体抛出的异常由 `Thread` 兜住，只记日志。
- 析构函数调用 `stop()`，所以析构时线程一定已经收干净。
- 任务队列是**无界**的（`std::deque`）。需要背压时用 `BoundedBlockingQueue` 搭配，
  或参照它自己实现一个有界版本。

---

## 4. `current_thread`

线程局部信息（tid、线程名）。数据存在 `thread_local` 缓存里，读取无锁。

```cpp
int64_t tid = common::base::current_thread::id();
std::string name = common::base::current_thread::thread_name();
common::base::current_thread::set_thread_name("io-worker");
bool main = common::base::current_thread::is_main();
```

- `id()`：缓存的 tid。首次访问时通过 `syscall(SYS_gettid)` 取一次，之后直接读缓存。
- `set_thread_name(name)`：写入 32 字节的线程名缓冲，超长会被截断（`strncpy` + 手动补 `'\0'`）。
  线程名默认是 `"unKnown"`。
- `is_main()`：判断当前线程是不是主线程。基准是主线程的 tid —— Linux 上就是
  `getpid()`（主线程 tid == 进程 pid）；Windows 上没有这个等价关系，改为进程启动
  阶段由主线程捕获一次。

---

## 5. 并发队列与同步原语

### 5.1 `BlockingQueue` —— 无界

```cpp
common::base::BlockingQueue<std::string> q;
q.put("hello");                      // 阻塞直到入队（无界，实际不会阻塞）
std::string s = q.take();            // 阻塞直到有元素；队列已关闭则抛异常
std::optional<std::string> v = q.try_take();   // 非阻塞，空则 nullopt
q.shutdown();                        // 让阻塞在 take() 上的线程醒来
```

- `take()` 的等待条件是 `!q_.empty() || done_`。**只有"已关闭且队列为空"才抛
  `std::runtime_error("queue is closed")`**；关闭后队列里已有的元素仍会被正常取出。
- `shutdown()` 后 `put()` 仍然允许写入（不会被拒），但没人再取 —— 元素会留在队列里。
- `put` 有 `const T&` 和 `T&&` 两个重载。传字符串字面量不会歧义（`T&&` 重载更优）。
- **销毁前必须先 `shutdown()`，再 join 所有可能阻塞在 `take()` 中的消费者线程。**
  析构函数是 `= default`，不做任何唤醒：`notify_all()` 唤醒的线程还要重新去抢 `mtx_`，
  那时 mutex 可能已经被析构。只 `shutdown()` 而不 join，销毁仍是未定义行为。
- 本类**不可拷贝也不可移动**：成员 `std::mutex` / `std::condition_variable` 都不可移动
  （移动 mutex 等于"复制一份锁状态"，副本和新对象不是同一把锁）。所以移动构造/赋值写的是
  `= delete` 而不是 `= default` —— `= default` 在首次声明时若隐式定义会 ill-formed，会被
  **静默定义成 deleted**，不报错、不拒绝编译，诊断推迟到使用点才炸。
  连带影响：不能放进 `std::vector`，不能作为可移动类的成员，也不能按值返回。

### 5.2 `BoundedBlockingQueue` —— 定长环形缓冲

```cpp
common::base::BoundedBlockingQueue<Task> q(16);
q.push(task);                        // 满时阻塞
bool ok = q.try_push(task);          // 满时返回 false
std::optional<Task> t = q.pop();     // 空时阻塞
std::optional<Task> t2 = q.try_pop_for(std::chrono::milliseconds(50));  // 超时返回 nullopt
q.stop();                            // 唤醒所有等待者
```

- 内部是 `std::vector<T>` 当环形缓冲，所以 **`T` 必须可默认构造且可拷贝赋值**。
- `stop()` 之后：`push()` 直接静默返回、`pop()` 返回 `nullopt`、`try_pop_for()` 返回 `nullopt`。
- 析构函数调用 `stop()`。

### 5.3 `CountDownLatch` —— 计数门闩

```cpp
common::base::CountDownLatch latch(3);
latch.count_down();                  // 减到 0 时唤醒所有等待者
latch.wait();                        // 阻塞到计数为 0
bool reached = latch.wait_for(std::chrono::milliseconds(100));
std::ptrdiff_t n = latch.get_count();
```

- 构造时计数为负会抛 `std::invalid_argument`。
- 计数已经到 0 之后再 `count_down()` 是安全的空操作（不会变负）。

---

## 6. `Timestamp`

微秒精度的时间点。

```cpp
common::base::Timestamp t = common::base::Timestamp::now();
int64_t us = t.micro_seconds_since_epoch();
std::string s = t.to_string();               // "1712345678.123456"
std::string f = t.to_formatted_string();     // "20240405 12:34:56.123456"
double seconds = common::base::time_difference(later, earlier);
common::base::Timestamp shifted = common::base::add_time(t, 1.5);
```

### 语义与注意事项

- **`to_formatted_string()` 输出的是 UTC**（内部用 `gmtime_r`），不是本地时间。
  格式为 `YYYYMMDD HH:MM:SS.uuuuuu`。
- **`now()` 用 `gettimeofday`，也就是 `CLOCK_REALTIME` —— 不是单调时钟**。
  系统时间被 NTP 调整或管理员手动改动时，`time_difference` 的结果可能为负。
  测量"经过了多久"应该用 `std::chrono::steady_clock`，不要用 `Timestamp`。
- `to_string()` 输出的是"秒.微秒"两段式，不是日期。
- `valid()` 的判断是 `micro_seconds_since_epoch_ > 0`；`invalid()` 返回 0 值。
- 析构函数是 `virtual` 的，但这个类没有任何多态用法 —— 值类型不需要虚析构，
  加了只会引入 vptr 和对象体积开销。
- 头文件依赖 `boost/operators.hpp` 提供的 `boost::totally_ordered1` 来自动生成
  `<`、`>`、`<=`、`>=`。这是 `deps/common/README.md` 第 8 节记录的技术债 ——
  为了一个运算符引入了整个 boost。
- `time_difference` 和 `add_time` 是**自由函数**而不是成员，与类内 API 风格不一致。
- `add_time` 的第二个参数是 `double` 秒，内部转成微秒时会截断，不是四舍五入。

---

## 7. `demangle`

把 C++ 修饰符号名还原成人能读的名字。

```cpp
int status = 0;
std::string name = common::base::demangle("_ZNSt3__14cerrE", status);   // status != 0 表示失败
std::string name2 = common::base::demangle("_ZNSt3__14cerrE");          // 简化版，忽略 status

common::base::DemangleResult r =
    common::base::try_demangle("_ZNSt3__14cerrE");  // unique_ptr<char, FreeingDeleter>
```

- 基于 `abi::__cxa_demangle`。**去混淆失败时返回原始名字**，不会抛异常。
- `DemangleResult` 是 `std::unique_ptr<char, FreeingDeleter>` —— `__cxa_demangle`
  返回的是 `malloc` 出来的 C 字符串，必须用 `free()` 释放，所以要自带 deleter。
- `try_demangle` 失败时返回空指针。
- 传入 `nullptr` 是未定义行为。

---

## 8. 已确认的问题

### 8.1 其余注意事项

| 位置 | 事项 |
| --- | --- |
| `timestamp.cpp` | `now()` 用 `CLOCK_REALTIME`，非单调，不能用来测时间间隔 |
| `timestamp.h` | `virtual ~Timestamp()` —— 值类型不需要虚析构 |
| `timestamp.h` | 为 `boost::totally_ordered1` 引入整个 boost（README 第 8 节技术债） |
| `thread_pool.cpp` | `stop()` 持有 `mutex_` 时 `notify_all()`，等待者被唤醒后还要再抢一次锁 |
| `count_down_latch.h` | 同一个计数同时用 `std::mutex` 和 `std::atomic` 保护，atomic 是冗余的 |
| `thread.cpp` | 线程体异常被 `catch(...)` 吞掉，只记日志 |
| `bounded_blocking_queue.h` | `max_size_ == 0` 时 `is_full_locked()` 恒为 true，`push()` 会永久阻塞（无参数校验） |
| `bounded_blocking_queue.h` | `try_pop_for()` 在"已 stop 且队列非空"时返回 `nullopt`，会丢掉一个元素 |
| `current_thread.cpp` | `_WIN32` 分支是刻意保留的：`deps/common` 是跨平台组件库，MOBAN 仓库自身只维护 Linux |
| `demangle.h` | 传入 `nullptr` 是未定义行为 |

---

## 9. 测试覆盖现状

测试在 `deps/common/tests/`，文件名 `<模块>_test.cpp`。

| 测试 | 用例数 | 覆盖内容 |
| --- | --- | --- |
| `block_queue_test.cpp` | 5 | 顺序存取、shutdown 后抛异常、try_take、多生产者多消费者、size/empty |
| `bounded_blocking_queue_test.cpp` | 5 | push/pop、满时 try_push 失败、try_pop_for 超时、并发、stop 唤醒阻塞者 |
| `count_down_latch_test.cpp` | 5 | 单线程、等到 0、超时返回、多等待者、超额 count_down |
| `thread_pool_test.cpp` | 5 | 启停、任务顺序、并发提交、任务抛异常、停止后投递被拒 |
| `timestamp_test.cpp` | 25 | 构造、比较、格式化、时间差、边界值 |
| `demangle_test.cpp` | 2 | `demangle` / `try_demangle` |
| `current_thread_test.cpp` | 5 | `id()` 与实时 tid 一致、`is_main()` 在主线程为真/子线程为假、线程名读写与截断 |

**没有测试的**：

- `Thread` —— 没有独立测试（通过 `thread_pool_test` 间接覆盖）
- `BlockingQueue` 的多生产者多消费者只在 `block_queue_test` 里有，
  `BoundedBlockingQueue` 的并发测试也只覆盖了基本路径

---

## 10. 与待补齐清单的对应

`deps/common/README.md` 第 9 节的 P2 项 `base/sync`（Mutex / RWLock / ConditionVariable /
SpinLock 薄封装、ThreadLocal）—— 本目录已经有 `current_thread`（ThreadLocal 的雏形）和
`CountDownLatch`（基于 `std::condition_variable`），但还缺 `Mutex` / `RWLock` 的薄封装。

Abseil 的 `absl/synchronization` 提供了 `absl::Mutex` / `absl::Notification` /
`absl::Barrier`，可以参考（见 `deps/third_party/abseil-cpp-cmake/README.md` 3.5 节）。
但 `std::mutex` 目前够用，是否引入值得单独评估 —— `common` 是底座，稳定性优先于性能。

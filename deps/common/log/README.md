# MOBAN Common Log

`deps/common/log/` 是日志门面：common 里**唯一**允许出现第三方日志库（spdlog）的地方。
对外暴露的是 `LOG_*` 宏和 `common::log` 命名空间下的一小组函数，第三方类型（`spdlog::logger`）
目前仍然出现在公共头文件里——这是 `deps/common/README.md` 第 4、8 节记录的技术债。

**命名空间**：`common::log`。调用方需显式限定，不提供全局别名。

---

## 1. 文件与职责

| 文件 | 内容 |
| --- | --- |
| `logging.h` | `LoggerType` 枚举、`LOG_*` 宏、级别转发函数、`clean_logfiles()` 声明 |
| `logging.cpp` | 默认 logger 的创建（静态初始化）、`register_logger` / `set_level` / `flush`、日志文件清理 |

依赖方向：`logging.cpp` → `logging.h` → spdlog。头文件里没有 `#include` 其他 common 模块。

---

## 2. 快速开始

```cpp
#include "log/logging.h"

LOG_INFO("server started on port {}", port);
LOG_WARN("peer {} closed", conn->name());
LOG_ERROR("read failed: {}", e.what());
```

格式串用 spdlog/fmt 的 `{}` 占位符，不是 `printf` 的 `%d`。同一参数可以既传纯值也可以传
带格式说明的值（`"x = {:0.2f}"`）。

---

## 3. 对外接口速查

### 3.1 `LoggerType`

```cpp
enum LoggerType {
  CONSOLE = 0,  // 控制台输出
  RUNTIME,      // 运行日志（落文件）

  NUM_LOGGERS
};
```

是无作用域枚举，写法是 `common::log::CONSOLE`。`NUM_LOGGERS` 是计数哨兵，不要当 logger 用。

### 3.2 取用与替换 logger

```cpp
spdlog::logger* l = common::log::get_logger(common::log::CONSOLE);
common::log::register_logger(common::log::CONSOLE, my_logger);
```

- `get_logger(type)` 返回**裸指针**，指向内部 `shared_ptr` 持有的对象。调用方不接管所有权。
- `register_logger(type, logger)` 用新 logger **替换**同类型的旧 logger；旧对象不会立刻销毁，
  而是被挪到一张"弃用表"里（见 5.5 节）。
- 两个内置 logger 在**静态初始化阶段**就创建好了（见 5.1 节），所以正常情况下
  `get_logger()` 永远不会返回 `nullptr`。

### 3.3 `set_level` / `flush`

```cpp
common::log::set_level(spdlog::level::debug);   // 打开 debug/trace
common::log::flush();                            // 立即把缓冲区落盘
```

- `set_level` 只作用于**内置的** CONSOLE / RUNTIME 两个 logger。自定义 logger 不受影响。
- 想用 spdlog 自己的 `spdlog::set_level()` 也可以，但那只对**注册在 spdlog 全局注册表里**的
  logger 生效：RUNTIME 是在注册表里的（`create_async` 会注册），CONSOLE 是直接构造的、
  不在注册表里——所以 `spdlog::set_level(debug)` 能打开文件的 debug，**打不开控制台的**。
- spdlog 的全局默认级别是 `info`，**`LOG_DEBUG` / `LOG_TRACE` 默认什么都不输出**，
  要看到它们必须显式 `set_level(spdlog::level::debug)`。
- `flush()` 遍历所有 logger 调用 flush；异步 logger 的 flush 是"往队列里塞一个 flush 消息"，
  不是同步等待落盘完成。

### 3.4 `clean_logfiles`

```cpp
common::log::clean_logfiles("./logs/logfile", 10);
common::log::clean_logfiles("./logs/logfile", 10,
                            [](std::string_view name) { return name.ends_with(".log"); });
```

- 目录不存在就**直接返回**，不报错。
- 只看**普通文件**，子目录跳过。
- 第三个参数是过滤器：返回 `true` 的文件才纳入统计。传空则全部普通文件都算。
- 文件数超限时按**文件名升序**排序，删掉最靠前的那些。对 `2026-10-23.log` 这种
  `%Y-%m-%d` 命名，字典序即时间序，成立。

---

## 4. 内置的两个 logger

| | CONSOLE | RUNTIME |
| --- | --- | --- |
| 载体 | `ansicolor_stderr_sink_mt`（Windows 用 `wincolor_stderr_sink_mt`） | `daily_file_format_sink_mt`（异步） |
| 输出 | 标准错误，带 ANSI 颜色 | `./logs/logfile/%Y-%m-%d.log` |
| 写入方式 | **同步**，调用方线程直接写 stderr | **异步**，丢给 spdlog 全局线程池 |
| 特殊设置 | debug 级别不着色 | `flush_on(info)`、保留 30 天 |

日志 pattern：

```text
# Debug 构建
%Y-%m-%d %H:%M:%S.%e %t %^%-5l%$ %s:%# %v
# Release 构建
%Y-%m-%d %H:%M:%S.%e %t %^%-5l%$ %v
```

Release 下 pattern 去掉了 `%s:%#`，宏里传的 `source_loc` 也是空的——**发布版本的日志没有文件名和行号**，
定位问题只能靠时间和内容。

两个 logger 的名字都带一个自增后缀（`console_1`、`runtime_1`）：spdlog 的全局注册表不允许重名，
重复注册同名 logger 会抛 `spdlog_ex`。后缀保证同一进程里多次构造不会撞名。

---

## 5. 语义与注意事项

### 5.1 静态初始化时机

`logging.cpp` 里的 `LogInitializer` 是**命名空间作用域的静态对象**，它在 `main` 之前构造，
构造时打开日志文件、注册两个 logger。

由此产生两条约束：

- **不要在别的翻译单元的静态初始化里打日志**。静态对象的构造顺序跨 TU 是未指定的，
  如果那个 TU 的初始化先跑，`get_logger()` 会返回 `nullptr`，解引用即崩。
- **日志目录的创建发生在静态初始化阶段**。如果这一步失败（磁盘满、权限不足），
  抛出的异常发生在 `main` 之前，栈展开已经结束，结果是 `std::terminate`，
  看不到任何有用的报错信息。

### 5.2 文件输出

- 路径 `./logs/logfile/%Y-%m-%d.log` 是**相对路径**，相对于进程的工作目录，不是可执行文件所在目录。
  从不同目录启动同一个程序会写到不同地方。
- 目录由 spdlog 自动创建（`create_dir` 会递归建所有层级），不需要预先 `mkdir`。
- **每天 0 点轮转**（`rotation_hour=0, rotation_minute=0`），文件名里的日期取自消息时间。
- **保留 30 天**（`max_files=30`）：轮转时删除最旧的那个，日志目录不会无限增长。
- 写入是异步的（见 5.3），进程正常退出时线程池会先把队列排空再结束；
  但 `abort()` / 崩溃 / `_exit()` 会丢掉队列里还没落盘的日志。

### 5.3 异步与刷盘

- 异步 logger 用的是 spdlog **全局线程池**：队列 8192 条，**1 个**工作线程。
- `create_async` 默认的溢出策略是 `block`：队列满时**调用方阻塞**直到有空位。
  日志写入速度跟不上生产速度时，业务线程会被日志拖住——这是有意为之（宁可慢，不要丢）。
  想改成"满了丢最旧的"要用 `create_async_nb`，本模块没用。
- `flush_on(info)`：每条 info 及以上的消息写完后**立即 flush**。
  好处是崩溃时日志不丢，代价是每行日志一次磁盘同步——高频写日志的场景要留意这个开销。
- console sink 是同步的，**每次 `LOG_*` 都有一次 stderr 写系统调用**。
  两类 logger 各写一份，磁盘上和控制台上是两份内容。

### 5.4 日志级别映射

| 宏 | spdlog 级别 |
| --- | --- |
| `LOG_TRACE` | `trace` |
| `LOG_DEBUG` | `debug` |
| `LOG_INFO` | `info` |
| `LOG_WARN` | `warn` |
| `LOG_ERROR` | `err` |
| `LOG_FATAL` | `critical` |

- spdlog 没有 `fatal` 级别，`LOG_FATAL` 映射到 `critical`。
- **`LOG_FATAL` 不会终止进程**，它只是一条日志。要终止得自己 `abort()`。
- 宏是变参的，至少要带一个参数（`LOG_INFO("x")` 可以，`LOG_INFO()` 不行）。

### 5.5 线程安全

- 读日志（`LOG_*`、`get_logger`）本身是线程安全的：logger 内部有锁，异步 logger 走队列。
- **`register_logger` 不是线程安全的**。内部两张表都是裸 `vector`，没有锁；
  换 logger 与读 logger 并发是数据竞争。
- 旧 logger 不会立刻销毁：换掉的旧对象被挪进"弃用表"（`disused_loggers`），至少活到下一次
  同类型替换为止。这样已经取到裸指针的线程不会因为对象被销毁而 use-after-free。
  但**同一类型只保留一个弃用对象**，反复替换同一类型时更早的旧对象还是会被释放——
  所以不要在运行期反复替换同一类型的 logger。
- `get_logger()` 返回裸指针，且级别转发函数里会**连续取两次**（控制台一次、文件一次），
  中间如果发生替换，两次拿到的可能不是同一个 logger。日志内容一致性问题可忽略，但别在这上面做假设。

### 5.6 头文件自包含与瘦身

`logging.h` 只用到 `spdlog::logger`、`spdlog::level::level_enum`、`spdlog::source_loc`，所以只
包含 `<spdlog/logger.h>`（`source_loc` 与 `level_enum` 由它包含的 `spdlog/common.h` 提供），
而不是 `<spdlog/spdlog.h>`——后者会额外拉入整个 sink 注册表。

直接用到的标准库类型也显式包含，不再依赖 spdlog 的传递包含：`<functional>`（`std::function`）、
`<memory>`（`std::shared_ptr`）、`<string>`、`<string_view>`、`<utility>`（`std::forward`）。

**这些 include 不增加编译时间**：它们本来就全部被 spdlog 传递包含，标准头的 include guard
让第二次 `#include` 成为空操作。按仓库实际的 `-DSPDLOG_COMPILED_LIB -DSPDLOG_FMT_EXTERNAL`
配置实测，加与不加这 5 行，预处理输出逐字节相同（均为 72030 行），耗时无差异。公共头真正的
编译成本 = 头文件体积 × 引用它的 TU 数量，与头文件里写了几行 `#include` 无关。真正会拖慢
编译的是往公共头里加**当前没被传递包含**的新重头。

收窄 spdlog 本身的收益有限但白拿：同一配置下每个引用 `logging.h` 的 TU 少解析约 1086 行
（73192 → 72106）、快约 45 ms（588 → 543 ms，三次取最优），仓库内 18 个引用方约省 0.8 s /
全量重建。剩下 7.2 万行里 fmt 占了约 4.2 万行，才是真正的大头，纯靠收窄 include 消不掉。

---

## 6. 已确认的问题

| 位置 | 事项 |
| --- | --- |
| `logging.h` | 公共头文件暴露 `spdlog::logger`、`spdlog::level`，违反"第三方类型不进公共头" |
| `logging.cpp` | `register_logger` 无锁，与并发读日志构成数据竞争 |
| `logging.cpp` | `LogInitializer` 在静态初始化阶段打开文件，失败即 `terminate` |
| `logging.cpp` | 日志路径相对 CWD，不同工作目录启动会写到不同位置 |
| `logging.cpp` | Release 构建丢弃文件名/行号 |
| `logging.cpp` | `LOG_FATAL` 不终止进程，与名字的直觉不符 |
| `logging.cpp` | 异步队列满时阻塞业务线程（策略是 `block`），高峰期会拖慢业务 |
| `logging.cpp` | `flush_on(info)` 每条日志一次 flush，高频日志时磁盘开销明显 |

---

## 7. 测试覆盖现状

**没有测试**。`deps/common/tests/` 下没有任何 log 相关的用例，`deps/common/CMakeLists.txt`
里也没有为 log 注册测试目标。

这个缺口比看起来重要，因为本模块的问题大多出现在**运行期**而不是编译期：

- 静态初始化时目录不存在 / 不可写时的行为；
- `register_logger` 替换后旧 logger 的生命周期；
- `clean_logfiles` 的过滤、排序、删除边界（文件数刚好等于上限、目录不存在、只读文件）；
- 异步队列满时是否真的会阻塞。

补测试时注意：日志目录是相对 CWD 的，测试不能依赖运行时工作目录
（`deps/common/README.md` 第 6 节），需要先把工作目录切到临时目录，或者给 `clean_logfiles`
传绝对路径。

---

## 8. 与待补齐清单的对应

`deps/common/README.md` 第 9 节的 P2 项 `log/context`——trace id 注入日志、采样——
本模块尚未实现。当前日志里只有线程 id（`%t`），没有一次请求的关联 id，
跨线程追踪同一次请求需要自己往消息里塞。

第 8 节的技术债"spdlog 暴露"也落在这里：把 `spdlog::logger` 换成自有类型后，
`register_logger` 的参数类型、`get_logger` 的返回类型都要改，
`set_level` 的参数类型同样要换成自有的级别枚举。

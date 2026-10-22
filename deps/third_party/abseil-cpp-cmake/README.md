# Abseil 常用模块速查

本目录是 Abseil 的 CMake 封装（`CMakeLists.txt` 是 Abseil 官方 `AbseilHelpers.cmake`
的精简版）。本文件记录 MOBAN 实际可能用到的 Abseil 模块，方便选型时少走弯路。

文中的所有代码片段都在本仓库当前的 vendored 快照上实际编译并运行验证过。

---

## 1. 当前状态

| 项 | 情况 |
| --- | --- |
| 快照版本 | `2024-06-11` 的 **tip-of-main**，**不是 LTS**（`absl/base/config.h` 中 `ABSL_LTS_RELEASE_VERSION` 被 `#undef`） |
| 目录 | `deps/third_party/abseil-cpp/`（源码），本目录只提供 CMake 接入 |
| 接入方式 | `deps/third_party/CMakeLists.txt` 里 `add_thirdparty(abseil-cpp-cmake abseil-cpp)` |
| 实际使用者 | 目前**零**。`deps/common/README.md` 第 4 节的依赖现状表尚未登记 Abseil |
| 构建 | `third_party/` 目录设了 `EXCLUDE_FROM_ALL`，只有被链接时才会真正编译 |

### 关于版本

因为不是 LTS，**没有 ABI 稳定性承诺**，upstream 后续可能引入 breaking change。
如果决定正式依赖 Abseil，建议把快照换成 LTS tag（如 `20240722`），把升级成本固定住。

### 关于准入

`deps/common/README.md` 第 2 节准入规则第 2 条：*"不引入新的重量级第三方依赖——
例外需要显式评审，并写进第 4 节的依赖现状表。"*

Abseil 已经 vendored 在仓库里，不算"新增第三方依赖"，但一旦在 `common/` 里实际使用，
仍需按规则更新第 4 节的依赖现状表。

### 关于这个封装目录

`CMakeLists.txt` 不是简单地 `add_subdirectory(abseil-cpp)`，而是把上游
`absl/*/CMakeLists.txt` 里的 `absl_cc_library` 调用**内联聚合**了一份（上游原文件不参与构建）。
文件头部的刷新规则写得很清楚：

```text
To refresh them when upgrading to a new version:
- copy them over from upstream
- remove calls of 'absl_cc_test'
- remove calls of `absl_cc_library` that contain `TESTONLY`
```

也就是说，**这个封装是按上游的划分来裁剪的**，据此使用要注意三点：

- **target 名单以本文件为准**，不要照抄上游 `absl/*/CMakeLists.txt` 里的 NAME ——
  两边可能对不上（比如 btree 的 target 叫 `absl::btree`，不是 `absl::btree_map`）。

- **上游标了 `TESTONLY` 的库不在这里**。上游一共标了 39 个（`absl::status_matchers`、
  `absl::hash_testing`、`absl::scoped_mock_log` 等），全是测试辅助库，
  按规则被移除。所以"某个头文件存在但没有对应 target"通常不是 bug，
  而是它本来就是测试专用（见 3.2 节的 `status_matchers`）。

- **升级 Abseil 快照后要重新核对 target 名单**，因为它是手工同步的。

判断某个头文件能不能用，最快的办法是查本文件里有没有对应的 `NAME`；
没有的话，要么它是被裁掉的 TESTONLY 库，要么它根本不属于 CMake 构建。

---

## 2. 快速上手

### 2.1 CMake 链接

封装里每个库都提供了 `absl::` 形式的别名 target：

```cmake
target_link_libraries(my_target PUBLIC
    absl::strings
    absl::status
)
```

常用 target 名见下文各模块的"CMake target"一行。

### 2.2 一个例子

```cpp
#include "absl/strings/str_cat.h"
#include "absl/status/statusor.h"

absl::StatusOr<int> ParsePort(const std::string& text) {
  int port = 0;
  if (!absl::SimpleAtoi(text, &port)) {
    return absl::InvalidArgumentError("bad port: " + text);
  }
  if (port <= 0 || port > 65535) {
    return absl::OutOfRangeError("port out of range");
  }
  return port;
}

const std::string message = absl::StrCat("port=", 8080);
```

---

## 3. 常用模块

### 3.1 `absl/strings` —— 字符串处理

**能干什么**：拼接、分割、连接、替换、格式化、大小写转换、去空白、ASCII 分类、
安全数字转换、转义、自定义类型格式化、零拷贝字符串。

| 能力 | 头文件 | 用法 |
| --- | --- | --- |
| 拼接 | `absl/strings/str_cat.h` | `absl::StrCat("a=", 1, ",b=", 2)` |
| 数字友好拼接 | `absl/strings/str_cat.h` | `absl::AlphaNum` —— `StrCat`/`StrAppend` 的参数类型，配 `Dec`/`Hex` |
| 分割 | `absl/strings/str_split.h` | `std::vector<std::string> v = absl::StrSplit("a,b,,c", ',');` |
| 连接 | `absl/strings/str_join.h` | `absl::StrJoin(parts, "\|")` |
| 替换 | `absl/strings/str_replace.h` | `absl::StrReplaceAll(s, subs)`，`subs` 是替换映射表 |
| 格式化 | `absl/strings/str_format.h` | `absl::StrFormat("%s=%d", "n", 3)` |
| ASCII 分类 | `absl/strings/ascii.h` | `absl::ascii_isalnum(c)`、`absl::ascii_isdigit(c)` 等 13 个 |
| 大小写 / 空白 | `absl/strings/ascii.h` | `absl::AsciiStrToLower`、`absl::AsciiStrToUpper`、`absl::StripAsciiWhitespace`（返回**视图**） |
| 安全数字转换 | `absl/strings/numbers.h` | `absl::SimpleAtoi/Atof/Atod/Atob(s, &v)`，返回 `bool` |
| 转义 | `absl/strings/escaping.h` | `absl::CEscape`、`absl::CHexEscape`、`absl::CUnescape` |
| 自定义类型格式化 | `absl/strings/has_absl_stringify.h` | `AbslStringify` 定制点 |
| 流式输出检测 | `absl/strings/has_ostream_operator.h` | `absl::HasOstreamOperator<T>` |
| 零拷贝字符串 | `absl/strings/cord.h` | `absl::Cord` |
| 字符串视图 | `absl/strings/string_view.h` | `absl::string_view` |

**CMake target**：`absl::strings` `absl::string_view` `absl::str_format` `absl::cord`

**注意**：

- `StrSplit` 返回的是惰性的 `Splitter`，**不是容器**，本身没有 `.size()`。要当容器用得
  显式赋值：`std::vector<std::string> v = absl::StrSplit("a,b,,c", ',');`

- `StrSplit` **保留空段**：`"a,b,,c"` 分成 4 段，不是 3 段。要跳过空段得自己过滤。

- `StrReplaceAll` **没有** `(s, old, new)` 这样的三参重载，必须给一张替换映射表：

```cpp
std::map<std::string, std::string> subs = {{"X", "-"}};
std::string result = absl::StrReplaceAll("aXa", subs);
```

  原地替换的变体参数顺序是**映射表在前、目标指针在后**，容易写反：

```cpp
int n = absl::StrReplaceAll(subs, &target);   // 返回替换次数
```

- `CUnescape` 是**出参风格**，返回 `bool` 表示成功与否：

```cpp
std::string unescaped;
if (!absl::CUnescape(src, &unescaped)) { /* 非法转义序列 */ }
```

- `StrFormat` 是编译期类型检查的，比 `snprintf` 安全；但比 `std::format` 慢（运行时解析格式串）。

- `StripAsciiWhitespace` 返回的是 **`absl::string_view` 视图**，指向输入串内部，不复制：

```cpp
absl::string_view v = absl::StripAsciiWhitespace("  x  ");        // 指向字面量，静态存储期，安全
absl::string_view w = absl::StripAsciiWhitespace(MakeString());   // 危险：临时串已销毁，视图悬垂
```

  要安全就自己存一份 `std::string`，或用原地版本 `absl::StripAsciiWhitespace(&str)`（返回 `void`）。
  `AsciiStrToLower` 则相反，返回新的 `std::string`。

- `SimpleAtoi` 是"转换成功返回 true"，不抛异常、不设置 errno —— 这点和 `std::stoi` 完全不同
  （`std::stoi` 失败抛 `std::invalid_argument`）。

- `absl::string_view` 就是 `std::string_view` 的别名，**没有 `.c_str()`**，
  要 `const char*` 得自己调 `std::string(sv).c_str()` 或用 `.data()`。

#### ASCII 分类函数 —— 最常被用到的，也最容易写错

`absl/strings/ascii.h` 提供 13 个分类函数和 2 个大小写转换：

```cpp
absl::ascii_isalnum(c);   absl::ascii_isalpha(c);   absl::ascii_isascii(c);
absl::ascii_isblank(c);   absl::ascii_iscntrl(c);   absl::ascii_isdigit(c);
absl::ascii_isgraph(c);   absl::ascii_islower(c);   absl::ascii_isprint(c);
absl::ascii_ispunct(c);   absl::ascii_isspace(c);   absl::ascii_isupper(c);
absl::ascii_isxdigit(c);
absl::ascii_tolower(c);   absl::ascii_toupper(c);   // 返回 char
```

**用它们代替手写的 `if (c >= 'a' && c <= 'z')`**，后者有三个问题：非 ASCII 字节
（UTF-8 的多字节序列）会被误判成字母；`char` 是有符号的，`c >= 'a'` 在负值上
会因整型提升得到错误结果；而 `std::isalpha` 之类的 `<ctype.h>` 函数受 locale 影响，
行为不可预测。

`ascii_*` 的语义是确定的：

- 参数类型是 `unsigned char`，返回 `bool`
- **字符值大于 127 一律返回 `false`** —— 只按 ASCII 判定，不看 locale
- 实现是查表（`ascii_internal::kPropertyBits`），**是 `inline bool` 而不是 `constexpr`**，
  所以不能用在需要编译期求值的场合

#### 数字转换 —— `SimpleAto*` 家族

```cpp
bool SimpleAtoi(absl::string_view str, int* out);      // base-10 整数
bool SimpleAtod(absl::string_view str, double* out);    // 双精度
bool SimpleAtof(absl::string_view str, float* out);     // 单精度
bool SimpleAtob(absl::string_view str, bool* out);      // "true"/"false"/"1"/"0"
```

语义（以 `SimpleAtoi` 为准）：

- 串的前后**允许有 ASCII 空白**，中间必须**整个**是一个合法数字（可带 `+`/`-`）
- 溢出或格式错误返回 `false`
- **失败时 `out` 处于未指定状态** —— 失败后不要读它

和 `std::stoi` 的区别：`std::stoi` 失败抛 `std::invalid_argument`、成功也不能区分
"转换了 0" 和 "失败"；`SimpleAtoi` 一律返回 `bool`，不抛异常、不设 errno。

#### 转义 —— `CEscape` / `CHexEscape` / `CUnescape`

```cpp
std::string CEscape(absl::string_view src);     // C 风格：\n \t \\ ，非打印字符转八进制
std::string CHexEscape(absl::string_view src);  // 十六进制风格
bool CUnescape(absl::string_view src, std::string* dest);  // 反转义，出参风格
```

`CUnescape` 是出参风格、返回 `bool`，反转义失败时 `dest` 不会被填充。

#### 自定义类型参与 `StrFormat` / `StrCat`

`AbslStringify` 是定制点。在类型自己的头文件里、同一命名空间下声明一个
**非成员**函数（通常用 friend 声明写在类里），签名固定：

```cpp
struct Point {
  int x;
  int y;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Point& p) {
    absl::Format(&sink, "(%v, %v)", p.x, p.y);
  }
};

std::string s = absl::StrFormat("%v", Point{1, 2});   // "(1, 2)"
```

`AbslStringify` 用 `%v` 作为底层转换符，`StrCat` 和 `StrFormat` 都认它。
`absl/strings/has_absl_stringify.h` 里的 `absl::HasAbslStringify<T>`
可以在编译期判断某个类型是否支持这个定制点；
`absl/strings/has_ostream_operator.h` 里的 `absl::HasOstreamOperator<T>`
则是判断它能不能被 `operator<<` 到 `std::ostream`。

#### `absl::Cord` —— 零拷贝字符串

`absl::Cord` 是一段字符序列，内部按"块"存储，因此**前置和后置追加都很便宜**，
多个 Cord 之间可以共享同一块内存。

**适合**：

- 大段字符串数据，在生命周期内会被反复追加 / 前置
- 需要跨 API 边界共享的大字符串（典型场景：HTTP 报文、protobuf）
- 从外部内存构造（`absl::MakeCordFromExternal`）

**不适合**：

- 存一般字符串数据 —— Cord 的构造有额外开销，**随机访问比 `std::string` 慢**
- 小字符串

常用操作：

```cpp
absl::Cord cord;
cord.Append("abc");
cord.Prepend("prefix");            // 前置很便宜
absl::Cord sub = cord.Subcord(3, 5);   // 取子串，不拷贝

std::string out;
absl::CopyCordToString(cord, &out);      // 拷成 std::string
absl::AppendCordToString(cord, &out);     // 追加进 std::string
```

`Cord` 还能估算自身占用的内存，粒度有三种（`absl::CordMemoryAccounting`）：
`kTotal`（共享内存重复计）、`kTotalMorePrecise`（去重，更贵）、`kFairShare`（按共享比例摊）。

**注意**：`Cord` 是"一种字符串容器"，不是字符串工具函数。除非确实有大字符串
拼接 / 共享的需求，否则用 `std::string` 更省心。

### 3.2 `absl/status` —— 错误状态

**能干什么**：`Status`（只有错误）和 `StatusOr<T>`（值或错误），用于替代"返回码 + 出参"风格。

| 能力 | 头文件 | 用法 |
| --- | --- | --- |
| 状态 | `absl/status/status.h` | `absl::Status`、`absl::OkStatus()` |
| 值或错误 | `absl/status/statusor.h` | `absl::StatusOr<T>` |
| 错误构造 | `absl/status/status.h` | `absl::InvalidArgumentError`、`absl::NotFoundError`、`absl::OutOfRangeError`、`absl::InternalError`、`absl::UnavailableError`、`absl::FailedPreconditionError`、`absl::AbortedError` |
| gtest 匹配器 | `absl/status/status_matchers.h` | `absl_testing::StatusIs(...)` |

**CMake target**：`absl::status` `absl::statusor`

**注意**：

- `StatusOr` 在本快照里位于 `absl/status/statusor.h`（2024 年中刚从 `absl/base/statusor.h`
  挪过来）。按旧路径 include 会找不到。

- `Status` 只能携带 code + message，**不能带任意上下文**（路径、字段名等只能拼进 message）。
  需要结构化上下文时得自己扩展。

- **`absl/status/status_matchers.h` 没有对应的 CMake target**。这是**上游的有意为之**：
  上游 `absl/status/CMakeLists.txt` 把 `status_matchers` 标成了 `TESTONLY`（它 `DEPS` 了
  `GTest::gmock` 和 `GTest::gtest`），而本封装的刷新规则明确写着
  *"remove calls of `absl_cc_library` that contain `TESTONLY`"*，所以它被移除了。

  换句话说：**Abseil 官方就把这组匹配器定位为"仅供测试使用"**，不是生产代码该依赖的东西。

  如果确实要用，两条路：

**A. 改用 gmock 原生匹配器**，作用在 `status.message()` 上（推荐，零额外依赖）：

```cpp
EXPECT_THAT(s.message(), ::testing::HasSubstr("nope"));
EXPECT_EQ(s.code(), absl::StatusCode::kNotFound);
```

**B. 自己补一个 target** 编译 `absl/status/internal/status_matchers.cc` 并链接 gmock。
这等于把上游标成 TESTONLY 的东西重新拉回生产依赖树，一般不值得。

匹配器本身在 `absl_testing` 命名空间，不是 `absl`：

```cpp
EXPECT_THAT(s, absl_testing::StatusIs(absl::StatusCode::kNotFound));
```

### 3.3 `absl/container` —— 容器

**能干什么**：比标准库更快 / 更省内存的哈希表和有序表，以及小对象免分配的 vector。

| 类型 | 头文件 | 相对标准库的改进 |
| --- | --- | --- |
| `absl::flat_hash_map` / `flat_hash_set` | `absl/container/flat_hash_map.h` | 比 `std::unordered_map` 快，迭代器在 rehash 时也不失效 |
| `absl::node_hash_map` / `node_hash_set` | `absl/container/node_hash_map.h` | 节点式，指针/引用稳定 |
| `absl::btree_map` / `btree_set` | `absl/container/btree_map.h` | 比 `std::map` 内存占用小得多 |
| `absl::InlinedVector` | `absl/container/inlined_vector.h` | 元素数少时不分配堆内存 |
| `absl::FixedArray` | `absl/container/fixed_array.h` | 栈上定长数组 |

**CMake target**：`absl::flat_hash_map` `absl::flat_hash_set` `absl::node_hash_map`
`absl::node_hash_set` `absl::btree`（btree_map 和 btree_set 共用这一个 target）
`absl::inlined_vector` `absl::fixed_array`

### 3.4 `absl/time` —— 时间

**能干什么**：时间点 / 时长的表示与运算，带时区的格式化。

| 能力 | 头文件 | 用法 |
| --- | --- | --- |
| 当前时间 | `absl/time/clock.h` | `absl::Now()` |
| 时间点 / 时长 | `absl/time/time.h` | `absl::Time`、`absl::Duration`、`absl::Seconds` |
| 格式化 | `absl/time/time.h` | `absl::FormatTime("%Y-%m-%d", t, absl::LocalTimeZone())` |
| 日历时间 | `absl/time/civil_time.h` | `absl::CivilSecond`、`absl::CivilTime` |

**CMake target**：`absl::time` `absl::civil_time`

**注意**：

- 本快照里 `absl::TimeZone` 和 `absl::LocalTimeZone()` 定义在 `absl/time/time.h` 中
  （`absl/time/time_zone.h` 已不存在），include 旧路径会失败。

- `FormatTime` 的**格式串是第一个参数**，容易写错顺序：
  `absl::FormatTime(fmt, time, tz)`。

- `absl::Duration` **没有 `.seconds()` 之类的方法**，要取整秒得用除法：

```cpp
absl::Duration d = absl::Seconds(3);
int64_t sec = d / absl::Seconds(1);        // 3
double  f   = absl::FDivDuration(d, absl::Seconds(1));  // 3.0
```

### 3.5 `absl/synchronization` —— 同步原语

**能干什么**：互斥锁、条件变量、屏障等。

| 类型 | 头文件 |
| --- | --- |
| `absl::Mutex` / `absl::MutexLock` / `absl::ReaderMutexLock` / `absl::WriterMutexLock` | `absl/synchronization/mutex.h` |
| `absl::Notification` | `absl/synchronization/notification.h` |
| `absl::Barrier` | `absl/synchronization/barrier.h` |
| `absl::BlockingCounter` | `absl/synchronization/blocking_counter.h` |

**CMake target**：`absl::synchronization`

**注意**：`absl::Mutex` 比 `std::mutex` 多了注解支持和慢模式下的死锁检测，
但接口比 `std::mutex` 复杂（读锁/写锁分开）。简单场景 `std::mutex` 仍然够用。

### 3.6 `absl/cleanup` —— 作用域清理

```cpp
#include "absl/cleanup/cleanup.h"

absl::Cleanup cleanup([&] { ::close(fd); });
// 离开作用域自动执行
```

C++23 的 `std::scope_exit` 之前，这是最干净的作用域守卫写法。

**CMake target**：`absl::cleanup`

### 3.7 `absl/log` —— 只建议用 CHECK

**能干什么**：`ABSL_CHECK` / `ABSL_CHECK_EQ` 等断言，失败直接 abort。

| 能力 | 头文件 | 说明 |
| --- | --- | --- |
| 致命检查 | `absl/log/absl_check.h` | `ABSL_CHECK`、`ABSL_CHECK_EQ`/`NE`/`LE`/`LT`/`GE`/`GT` |
| 退出式检查 | `absl/log/absl_check.h` | `ABSL_QCHECK*`，失败直接终止进程 |
| 仅调试检查 | `absl/log/absl_check.h` | `ABSL_DCHECK*`，只在 debug 编译下生效 |
| 异常式检查 | `absl/log/check.h` | `CHECK(...)`、`CHECK_EQ(...)` 等，失败抛异常 |
| 日志输出 | `absl/log/absl_log.h` | `ABSL_LOG(INFO) << ...`；严重级别常量在 `absl/base/log_severity.h` |
| 结构化日志 | `absl/log/structured.h` | 结构化字段 |

**CMake target**：`absl::absl_check` `absl::check` `absl::absl_log` `absl::log_structured`

**注意**：

- 本快照**没有 `ABSL_ASSERT_*` 这一族宏**。可关闭的断言叫 `ABSL_DCHECK_*`（debug-only）。
- `ABSL_CHECK` 系列的宏定义在 `absl/log/absl_check.h`（不含异常版本），
  `absl/log/check.h` 提供的是另一套 `CHECK*`（失败抛异常）。两者别混用。
- `ABSL_LOG` 在 `absl/log/absl_log.h`，**严重级别常量（`INFO` 等）在
  `absl/base/log_severity.h`** —— 只 include `absl/log/structured.h` 是编译不过的。
- `ABSL_LOG` 默认写 **stderr**，且在调用 `absl::InitializeLog()` 之前会打一行警告。
  要接管输出得自己注册 `absl::LogSink`。
- MOBAN 的日志门面是 spdlog（见 `deps/common/README.md` 第 4 节），
  **不要**引入 Abseil 的日志输出能力，两者混用会产生两套日志格式和两套配置。
  这里只取 `ABSL_CHECK` 用作断言 —— 它比 `assert` 强的地方是 **NDEBUG 下不会被消掉**。

### 3.8 `absl/flags` —— 命令行参数

```cpp
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

ABSL_FLAG(int, port, 21, "监听端口");
ABSL_FLAG(std::string, root, ".", "根目录");

// main 里
absl::ParseCommandLine(argc, argv);
```

**CMake target**：`absl::flags` `absl::flags_parse`

**注意**：flag 是全局注册的单例，只能定义在全局命名空间，不能放在匿名 namespace 里。

### 3.9 `absl/types` —— 类型工具

| 类型 | 头文件 | 说明 |
| --- | --- | --- |
| `absl::optional` | `absl/types/optional.h` | `std::optional` 的前身，现在 std 已有，意义不大 |
| `absl::variant` | `absl/types/variant.h` | `std::variant` 的前身 |
| `absl::any` | `absl/types/any.h` | 类型擦除容器 |
| `absl::Span` | `absl/types/span.h` | 连续内存的视图，对应 C++20 `std::span` |
| `absl::compare` | `absl/types/compare.h` | 三路比较辅助 |

**CMake target**：`absl::optional` `absl::variant` `absl::any` `absl::span`

**注意**：C++17 上 `optional` / `variant` 直接用 `std::` 版本即可，Abseil 的版本没必要。
`absl::Span` 倒是可以考虑，用来替代 C++20 才有的 `std::span`。

### 3.10 `absl/crc`、`absl/random`、`absl/numeric`、`absl/algorithm`

| 模块 | 头文件 | 能干什么 | CMake target |
| --- | --- | --- | --- |
| `absl/crc` | `absl/crc/crc32c.h` | `absl::ComputeCrc32c(buf)` 算 CRC32C 校验和（比 CRC32 更快），返回值是强类型 `absl::crc32c_t`；`absl::ExtendCrc32c` 可增量计算 | `absl::crc32c` |
| `absl/random` | `absl/random/random.h` | 可复现的随机数流 + 各种分布（`absl::UniformDistribution` 等） | `absl::random_distributions` |
| `absl/numeric` | `absl/numeric/int128.h` | `absl::int128` / `absl::uint128` 128 位整数 | `absl::int128` |
| `absl/algorithm` | `absl/algorithm/container.h` | C++20 ranges 的提前版本，容器版算法 `absl::c_accumulate` / `absl::c_sort` 等 | `absl::algorithm_container`（`absl/algorithm/algorithm.h` 则是 ranges 概念那一半，对应 `absl::algorithm`） |

`absl/numeric` 在本快照里**只有 `int128`**，没有 `SafeAdd` / `SafeMul` 之类的溢出安全算术
（C++20 的 `std::add_overflow` 才是那个功能）。

### 3.11 不建议直接依赖的模块

| 模块 | 能干什么 | 为什么不建议 |
| --- | --- | --- |
| `absl/base` | 内部实现细节（config、macros、annotations） | Abseil 内部实现，官方不建议业务代码直接 include |
| `absl/meta` | 类型_traits 扩展 | 同上 |
| `absl/debugging` | `absl::StackTrace` 栈回溯、`absl::Symbolize` 符号化 | 内部实现；栈回溯可以用 `backtrace()` 替代 |
| `absl/copts` | 编译选项集合 | 不该直接依赖 |
| `absl/utility` | `absl::NullValueComparator` 等小工具 | 零散小工具，收益不大 |

真要用 `absl::StackTrace` 之类，优先确认对应头文件在封装里是否真的有 target，
没有的话同样会遇到 `status_matchers` 那种"头文件在、实现没编"的问题。

---

## 4. 四个必须知道的坑

### 4.1 头文件不传递依赖

`#include "absl/strings/str_cat.h"` **不会**顺带引入 `absl/strings/string_view.h`。
Abseil 要求每个用到的头文件都自己 include —— 这和标准库的习惯相反，
是接入 Abseil 时最常见的编译错误来源。

### 4.2 不能让 Abseil 类型漏进公共头文件

`deps/common/README.md` 第 3 节：*"第三方库进 common 必须先封装：
common 的公共头文件里不应出现第三方类型。"*

因此 `absl::flat_hash_map` 用在 `common/` 的实现文件里没问题，但出现在
`common::util::xxx.h` 的函数签名里就违规了 —— 必须包一层自己的类型。

### 4.3 API 会随版本漂移

本快照里已经能看出搬迁痕迹：

- `absl::StatusOr`：`absl/base/statusor.h` → `absl/status/statusor.h`
- `absl::TimeZone`：`absl/time/time_zone.h` → 并入 `absl/time/time.h`
- `absl::Span`：`absl/memory/span.h` → `absl/types/span.h`

按记忆里的旧路径写 include 会直接编译失败。升级快照时要以实际头文件为准。

### 4.4 `Status` 承载不了结构化上下文

`absl::Status` 只能带 code + message。需要携带路径、字段名这类定位信息时，
只能把信息拼进 message 字符串 —— 这会损失结构化程度。
如果 MOBAN 的 `base/error` 需要更强的错误表达，得在 `common::base` 里自己扩展一层。

---

## 5. 对 MOBAN 的落地建议

按 `deps/common/README.md` 第 9 节"待补齐清单"对齐：

| README 待补齐项 | 建议 |
| --- | --- |
| P0 `utilities/string_utils` | **优先用 `absl/strings`**。`StrCat`/`StrSplit`/`StrJoin`/`StrReplaceAll`/`SimpleAtoi` 正好覆盖清单里列的能力，且 `SimpleAtoi` 就是"带错误检查的安全数字转换" |
| P1 `base/error` | **用 `absl/status` 做底座**，在 `common::base` 里包一层自己的 `Status` / `Result<T>`，公共头文件不暴露 `absl::` 类型（见 4.2） |
| P0 `utilities/path_utils` | `absl/strings` 帮不上忙，路径拼接用 `std::filesystem::path` 即可 |
| P2 `base/sync` | `absl/synchronization` 可用，但 `std::mutex` 目前够用，不急 |
| 技术债：`timestamp.h` 引入整个 boost | `absl/time` 可以替掉 `boost::totally_ordered1`，且带时区格式化能力 |

**暂时不建议碰的**：`absl/container`（换过去主要收益是性能，而 `common` 是底座，
稳定性优先）、`absl/log` 的日志能力（会和 spdlog 冲突）、`absl/base` 内部头文件。

**依赖引入顺序建议**：先 `absl/strings`（能直接替掉一半 `string_utils` 的活），
再 `absl/status`（支撑 P1 `base/error`），最后评估 `absl/time` 能否解掉 boost 依赖。
每引入一个，按 README 第 2 节更新第 4 节的依赖现状表。

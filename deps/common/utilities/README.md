# MOBAN Common Utilities

`deps/common/utilities/` 是 common 里与业务无关的小工具集合：字符串处理、文件读写，
以及几个历史遗留的 RAII / 宏 / 单例头文件。

**命名空间**：`common::util`（`string_utils.h`、`file_utils.h`）。
`defer.h`、`noncopyable.h`、`short_file.h`、`singleton.h` 四个遗留头仍在**全局命名空间**，
详见第 5 节。

按 `deps/common/README.md` 第 2 节的准入规则，本目录的定位是"通用组件"，
不引入重量级第三方依赖：`string_utils` 只用标准库，`file_utils` 只用 POSIX 与
`<filesystem>`。

---

## 1. 文件与职责

| 文件 | 内容 | 依赖 |
| --- | --- | --- |
| `string_utils.h` / `.cpp` | 裁剪、切分、拼接、大小写、前后缀、替换、数字与布尔转换 | 仅标准库 |
| `file_utils.h` / `.cpp` | 存在性与类型判断、文件大小、读写、原子替换、目录列举 | POSIX + `<filesystem>` |
| `defer.h` | 作用域退出时执行清理的 `defer` 模板与 `DEFER` 宏 | `<utility>` |
| `noncopyable.h` | 禁拷贝基类与 `DISALLOW_COPY*` 宏 | 无 |
| `short_file.h` | `SHORT_FILE` 宏，取 `__FILE__` 的最后一段 | 无 |
| `singleton.h` | CRTP 单例模板 | `noncopyable.h` |

依赖方向：四个遗留头之间只有 `singleton.h` → `noncopyable.h` 一条边；
`string_utils` 与 `file_utils` 互不依赖，也不依赖 common 的其他模块。

---

## 2. 快速开始

```cpp
#include "utilities/string_utils.h"
#include "utilities/file_utils.h"

const auto trimmed = common::util::trim(raw);          // std::string_view
const auto parts = common::util::split(line, ',');      // vector<string_view>
const std::string joined = common::util::join(parts, ",");

if (common::util::exists(path)) {
  const std::string body = common::util::read_file(path);
  common::util::write_file_atomic(path, body);
}
```

调用方一律写完整限定名 `common::util::`，不提供全局别名（`deps/common/README.md` 第 5 节）。

---

## 3. 对外接口速查

### 3.1 字符串工具（`common::util`）

| 函数 | 返回 | 说明 |
| --- | --- | --- |
| `ltrim` / `rtrim` / `trim` | `std::string_view` | 裁掉首尾的 **ASCII 空白**（`' '` `\t` `\n` `\v` `\f` `\r`） |
| `split(input, delimiter)` | `vector<std::string_view>` | 按分隔符切分 |
| `join(range, delimiter)` | `std::string` | 连接；元素可以是 `string` / `string_view` / `const char*` |
| `to_lower` / `to_upper` | `std::string` | ASCII 大小写转换 |
| `iequals` / `istarts_with` / `iends_with` | `bool` | 大小写无关的比较 |
| `starts_with` / `ends_with` | `bool` | 大小写敏感的前后缀判断 |
| `replace_all` / `replace_first` | `std::string` | 字符串替换 |
| `to_int64` / `to_uint64` / `to_double` / `to_bool` | 对应类型 | 严格解析，失败抛异常 |
| `try_to_int64` / `try_to_uint64` | `std::optional` | 不抛异常的整数解析，`noexcept` |
| `try_to_double` / `try_to_bool` | `std::optional` | 不抛异常的浮点/布尔解析 |

**空白只认 ASCII**：不用 `std::isspace`——它受全局 locale 影响，且对负值 `char` 是未定义行为。

**大小写转换只按字节处理**：协议字段、配置项这类标识符基本都是 ASCII，
按字节处理不会把多字节字符拆坏，也不引入 locale 依赖。

**异常约定**：

| 情况 | 行为 |
| --- | --- |
| `split` 的 `delimiter` 为空 | 抛 `std::invalid_argument`（空分隔符没有切分语义） |
| `replace_all` / `replace_first` 的 `from` 为空 | 抛 `std::invalid_argument`（替换空串没有可定义语义，静默返回原串只会掩盖调用方的 bug） |
| 数字格式非法 | 抛 `std::invalid_argument` |
| 数字越界 | 抛 `std::out_of_range` |

异常消息格式为 `[context: ]invalid <type> value: '<input>'` 或
`[context: ]value out of range for <type>: '<input>'`。`context` 是可选的定位信息
（配置文件里传字段路径），会出现在消息里。

### 3.2 文件工具（`common::util`）

| 函数 | 返回 | 失败行为 |
| --- | --- | --- |
| `exists` / `is_directory` / `is_regular_file` / `is_symlink` | `bool` | **不抛**，查询失败一律当"不存在" |
| `file_size` | `uint64_t` | 抛 `std::system_error` |
| `read_file` / `read_file_bytes` | `std::string` / `vector<uint8_t>` | 抛 `std::system_error` |
| `write_file` / `write_file_bytes` | `void` | 抛 `std::system_error` |
| `write_file_atomic` | `void` | 抛 `std::system_error` |
| `list_directory` | `vector<std::string>` | 抛 `std::system_error` |

**路径参数用 `const std::string&`**：`std::string` 保证以 NUL 结尾，可以直接交给
`open(2)` / `stat(2)`，实现里不用再物化一份。

**符号链接的跟随规则不一致，是有意的**：

| 函数 | 是否跟随软链接 |
| --- | --- |
| `exists` | 跟随——指向不存在目标的软链接返回 `false` |
| `is_directory` / `is_regular_file` | 跟随——指向目录的软链接返回 `true` |
| `is_symlink` | **不跟随**——软链接指向不存在的目标也为 `true` |

### 3.3 遗留头文件

| 头 | 内容 | 仓库内使用情况 |
| --- | --- | --- |
| `defer.h` | `defer<F>` 模板、`make_defer()`、`DEFER` 宏 | **无人使用** |
| `noncopyable.h` | `noncopyable` 基类、`DISALLOW_COPY*` 宏 | 被 `plugins/Muduo` 的 9 个头引用 |
| `short_file.h` | `SHORT_FILE` 宏、`past_last_slash()` | **无人使用** |
| `singleton.h` | `Singleton<T>` CRTP 单例 | **无人使用**（仅被自己依赖） |

`defer` 的两种用法：

```cpp
void do_file_io(const std::string& filename) {
  auto fp = std::fopen(filename.c_str(), "r");

  // 方法一：宏
  DEFER({ std::fclose(fp); });

  // 方法二：显式对象
  const auto& defer_close = make_defer([&fp]() { std::fclose(fp); });
}
```

与 Go 的 `defer` 不同：这里的清理函数在**离开当前作用域**时调用，而不是函数返回时。
`defer` 的拷贝/移动构造函数都被删除——否则清理函数会被调用多次。

---

## 4. 语义与注意事项

### 4.1 返回的视图指向入参

`ltrim` / `rtrim` / `trim` / `split` 返回的 `std::string_view` 指向入参内部，不发生拷贝。
调用方必须保证入参的底层存储比返回值活得久——**不要对临时 `std::string` 调用**：

```cpp
auto v = common::util::trim(std::string("  x  "));   // 悬垂：临时对象已析构
std::string s = "  x  ";
auto w = common::util::trim(s);                       // OK
```

`split` 的返回容器同理，元素全部指向入参。

### 4.2 切分的语义约定

- 连续分隔符产生空字段：`"a,,b"` → `{"a", "", "b"}`，不静默丢弃；
- 空输入返回**单个空字段**，保证"字段数 = 分隔符数 + 1"；
- 分隔符可以是多字符。

### 4.3 数字解析：格式优先，范围其次

判定顺序是固定的：

1. 串尾有没消费掉的字符（`"12x"`、`"1e400x"`）→ **格式非法**；
2. 格式合法但数值放不进目标类型 → **越界**。

`"99999999999999999999x"` 报格式非法而不是越界：多出来的那个 `'x'` 才是真正的问题，
报越界会让人去改数字大小，改完还是错。

浮点数的具体规则：

| 输入 | 结果 |
| --- | --- |
| `"0x1p3"` | 格式非法——十六进制浮点不是这里接受的字面量 |
| `"inf"` / `"nan"` / `"nan(...)"` | 格式非法——不同工具链的解析器行为不一致，必须自己挡 |
| `"5e-324"` | 正常值（subnormal 是完全可表示的数） |
| `"1e400"` | 越界（上溢） |
| `"1e-400"` | 越界（下溢到 0，精度已丢失） |

`to_bool` 接受（大小写不敏感）`true`/`false`、`1`/`0`、`yes`/`no`、`on`/`off`。

`try_to_double` / `try_to_bool` **不是** `noexcept`：它们要构造临时字符串，分配失败会抛
`std::bad_alloc`。两个整数版本不分配内存，所以标了 `noexcept`。

### 4.4 为什么浮点用 `strtod_l` 而不是 `from_chars`

浮点解析走 `strtod_l` 并显式传 C locale：`strtod` 受 `LC_NUMERIC` 影响，程序一旦调用过
`setlocale`，小数点可能变成逗号，`"1.5"` 就解析不出来了。

十六进制浮点与 `inf`/`nan` 是**显式挡掉**的，不能依赖标准库的解析器去拒绝：
libstdc++ 的流解析会拒绝（字符集里没有 `i`/`n`），libc++ 内部走 `strtod` 则会解析成
infinity——同一个输入在不同工具链下结果不同。

> `string_utils.cpp` 里留了一条 TODO：等工具链升到 GCC 11+ 就用 `std::from_chars` 的浮点
> 版本替掉 `strtod_l`。**这条注释的前提已经变了**——它写的是"本机还是 GCC 9.4"，
> 而现在默认 `g++` 已经是 15.2，CI（ubuntu 22.04 容器）也是 11，两者都支持浮点
> `from_chars`；只有显式用 `g++-9` 编译才是例外。见第 5 节。

### 4.5 判断类函数不抛异常，读取/写入类抛

判断类函数（`exists` / `is_*`）把查询失败（权限不足、路径非法、中间目录不存在）
一律当作"不存在"返回 `false`。这与 `std::filesystem` 的约定一致，也因为"查不到"和
"不在"对调用方的意义相同——真要区分，用 `file_size` / `read_file` 那类会抛异常的函数。

真正要读写的函数失败一律抛 `std::system_error`，消息里带完整路径和底层 errno 原因，
最终形如 `cannot read file '/tmp/x': No such file or directory`。内存不足也统一成
`std::system_error`，调用方只需要处理一种异常类型。

### 4.6 读写的一些边界

- `read_file` 不限于普通文件：设备文件（`/dev/null`）、`/proc` 下的伪文件都能读。
  目录会读失败（`Is a directory`）。命名管道（FIFO）也能读，但按 POSIX 语义，
  `open` 会一直阻塞到有写端打开为止——这是 FIFO 本身的语义。
- `file_size` 按 `std::filesystem` 的语义，只有普通文件才有大小。设备文件、管道、
  `/proc` 下的伪文件会抛 `std::system_error`（`Operation not supported`），**这不是 bug**。
- `write_file` **不是**原子写：写到一半失败会留下半截内容。它也**不做 fsync**，
  内容可能还在页缓存里。目标已存在时沿用原文件权限；新建文件按 `0666` 交给 umask 裁剪。
- `list_directory` 只返回文件名（不含目录前缀），不递归，不含 `.` 和 `..`，
  顺序是文件系统顺序、**不保证排序**，需要有序结果请自行排序。

### 4.7 原子写保证了什么

`write_file_atomic` 的做法是：在目标**同目录**下写一个临时文件，`fsync` 落盘后用
`rename(2)` 替换目标。

- 同目录是必须的：`rename` 只有在同一文件系统内才是原子的；
- 保证读取者要么看到旧内容、要么看到完整的新内容，不会看到写了一半的内容；
- 内容在 `rename` 之前已经 `fsync` 落盘，掉电后不会退化成空文件；
  `rename` 之后还会对父目录做一次 `fsync`（best effort），让目录项本身也落盘——
  这一步失败不影响返回值，但此时掉电持久性不再保证；
- 目标不存在时是创建；目标已存在时沿用原文件权限，不会因为替换就把 `0600` 的文件
  悄悄放宽成 `0644`。`setuid`/`setgid` 位**不继承**：原子替换写入的是新内容，
  保留 setuid 会让新内容以特权身份运行；
- 并发安全：对同一个目标路径，多个线程同时调用不会互相破坏，各自的临时文件互相独立，
  最终目标文件是其中一次写入的完整内容。

实现上还有两道防护：临时文件名带随机后缀（用 `O_EXCL` 独占创建，同目录的其他用户
猜不到名字）；`rename` 之前比对临时文件的 `dev`/`ino`，确认它没被换成别人的文件。

### 4.8 线程安全

- `string_utils` 的函数全部是无状态的纯函数，**所有函数都可以被多线程并发调用**；
- `file_utils` 的全部函数不持有可变状态，**可以并发调用**；但同一路径上的并发写
  不在保证范围内（需要"要么旧内容、要么完整新内容"就用 `write_file_atomic`）。

---

## 5. 已确认的问题

| 位置 | 事项 |
| --- | --- |
| `defer.h` | 头注释里的 `dt::make_defer` 与代码不符：实际是全局 `make_defer`，仓库里**没有** `dt` 命名空间，示例代码照抄编译不过 |
| `defer.h`、`noncopyable.h`、`short_file.h`、`singleton.h` | 类型与函数都在全局命名空间，违反 `deps/common/README.md` 第 5 节"统一用 `common::<模块>`"；第 8 节"命名空间已迁入"那行对这四个头并不成立 |
| `defer.h` | 类型名 `defer` 小写，违反第 5 节"类型 `PascalCase`" |
| `defer.h` | `DEFER` 宏用了 `__attribute__((always_inline))`，MSVC 下不兼容（仓库只维护 Linux，暂不影响） |
| `defer.h` | 清理函数在析构里调用；若在栈展开期间抛出异常，会直接 `std::terminate` |
| `short_file.h` | `past_last_slash` 声明为 `static constexpr`（头文件里的 internal linkage），每个 TU 一份副本；仓库内无人使用 |
| `defer.h`、`short_file.h`、`singleton.h` | 仓库内无任何使用者，违反第 2 节"有真实需求验证——不凭想象预先堆一批以后可能用的工具" |
| `string_utils.h` | `detail::` 里的实现细节（`is_ascii_whitespace` / `to_lower_ascii` / `as_string_view`）暴露在公共头里，调用方不应依赖 |
| `string_utils.cpp` | 浮点解析的 TODO 注释前提已变（"本机还是 GCC 9.4"），现在默认 `g++` 是 15.2、CI 是 11，都支持浮点 `from_chars` |
| `file_utils.cpp` | 局部 `ScopedFd` 重复实现了 `utilities/unique_fd`（P1 待补齐项），代码里已用 TODO 标注 |
| `file_utils.h` | 注释把"路径拼接与规范化"推给 `path_utils`，但 `path_utils` 尚未实现（P0） |
| `file_utils.h` | 原子写的临时文件逻辑没有对外暴露成可复用的 RAII 临时文件（P0 `utilities/temp_file`） |

---

## 6. 测试覆盖现状

| 测试文件 | 用例数 | 覆盖内容 |
| --- | --- | --- |
| `tests/string_utils_test.cpp` | 39 | 裁剪（含只去一侧、只认 ASCII 空白、返回视图指向入参）、切分（空字段、空输入、多字符分隔符、空分隔符抛异常）、拼接、大小写转换（非 ASCII 字节不受影响）、前后缀（大小写敏感/不敏感）、替换、数字解析（格式非法、越界、负数进无符号、subnormal、十六进制浮点、格式优先于范围、异常消息带 context 与类型名）、布尔解析 |
| `tests/file_utils_test.cpp` | 38 | 存在性与类型（含软链接不跟随、悬空软链接）、文件大小、读取（空文件、内嵌 NUL 字节、设备文件、`/proc` 伪文件、目录读失败、权限不足）、写入（覆盖、字节形式、目录不存在、保留原权限）、原子写（创建、替换、失败时保留原内容、不留临时文件、长文件名、保留权限、并发写不暴露半截内容）、目录列举（跳过 `.`/`..`、空目录、非目录与不存在路径抛异常且带路径） |

两个测试文件都被 `deps/common/CMakeLists.txt` 第 18 行的
`add_headers_and_sources(base_tests tests)` 收进**同一个** gtest 目标——目标名
`base_tests` 与模块对不上，utilities 的 77 个用例没有独立的测试目标。

**没有测试的部分**：`defer.h`、`noncopyable.h`、`short_file.h`、`singleton.h`
四个头完全没有用例。`defer` 的清理时机、宏展开的唯一性、`SHORT_FILE` 的路径截断
都属于容易写错又不容易被编译器发现的逻辑。

---

## 7. 与待补齐清单的对应

`deps/common/README.md` 第 9 节里，本目录相关的待补齐项：

| 优先级 | 项 | 现状 |
| --- | --- | --- |
| P0 | `utilities/path_utils` | 未实现。`file_utils.h` 的注释已经把路径拼接与规范化推给它 |
| P0 | `utilities/temp_file` | 未实现。原子写内部有临时文件逻辑，但没有对外的 RAII 临时文件/临时目录 |
| P1 | `utilities/unique_fd` | 未实现。`file_utils.cpp` 里的局部 `ScopedFd` 就是它的临时替身 |
| P1 | `utilities/encoding`（base64、hex） | 未实现 |
| P1 | `utilities/digest`（md5、sha256、crc32） | 未实现 |

另外第 8 节的技术债"目标划分"（单一 `lib_common` 静态库）在本目录体现为：
`string_utils` 与 `file_utils` 会被所有链接 `lib_common` 的目标一起带上，
但它们其实互不依赖、也没有对外的重依赖，拆分 target 时可以优先分离。

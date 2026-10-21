# MOBAN Common

`deps/common` 是 MOBAN 的公共底座：与具体业务无关的通用构件集合，既服务本仓库的
`example/`，也供后续新项目直接取用。

它的定位是**通用组件库**，不是"某个项目抽出来的代码"，也不是"第三方库的堆放处"。
下面的规则用于保证它长期保持可复用、可测试、可独立分发。

---

## 1. 目录结构

| 目录 | 职责 | 约束 |
| --- | --- | --- |
| `base/` | 进程级基础构件：线程、线程池、时间戳、并发队列 | 不依赖其他模块（`log/` 除外，日志属于横切能力） |
| `utilities/` | 与资源无关的小工具：RAII、宏、单例、字符串/文件工具 | 纯头文件优先，不得引入重量级依赖 |
| `log/` | 日志门面 | 唯一允许暴露第三方日志库的地方，且必须封装 |
| `config/` | 配置读取与格式后端 | 格式解析实现与配置读取逻辑分离，新增格式不改动核心 |
| `di/` | 加速器设备接口（CUDA / HIP / host）桩 | 唯一与硬件相关的目录，物理隔离，不得被通用代码依赖 |
| `tests/` | common 自身的单元测试 | 只测 common，不测业务 |

## 2. 准入规则

一个组件要进 common，必须**同时**满足下面四条：

1. **与业务无关**——任何项目都可能用到；代码里不应出现业务名词（`ftp`、`session`、
   `user` 之类）。
2. **不引入新的重量级第三方依赖**——例外需要显式评审，并写进第 4 节的依赖现状表。
3. **有真实需求验证**——不凭想象预先堆一批"以后可能用"的工具；先在业务里遇到，
   再上收。纯粹为了"补全底座"而写的东西不进 common。
4. **能独立测试**——不依赖全局可变状态、不依赖 `main`、不依赖运行时工作目录。

以下东西**不进** common：

| 内容 | 去处 |
| --- | --- |
| 业务逻辑、协议处理、会话管理 | `example/` 或未来的业务仓库 |
| 网络框架本体（如 Muduo） | `plugins/` |
| 硬件/加速器相关代码 | `di/` |
| 一次性脚本、实验代码 | 仓库根 `tools/` 或直接删除 |
| 第三方库的原始接口 | 先在 common 内封装一层再暴露 |

## 3. 依赖方向

```text
example/  ──►  plugins/  ──►  common/  ──►  deps/third_party/
```

- common **不得** include `plugins/` 或 `example/` 的任何头文件；这条由编译期依赖
  保证，不靠自觉。
- plugins 可以依赖 common，业务代码可以同时依赖两者。
- common 内部模块之间尽量单向：`base/` 不依赖 `config/`、`config/` 不依赖 `log/`
  （日志是横切能力，任何模块都可以用）。

## 4. 第三方库接入规则

- 优先复用 `deps/third_party/` 里已 vendored 的库；新增库先放这里，不要直接写进
  common 的源码目录。
- 第三方库进 common 必须**先封装**：common 的公共头文件里不应出现第三方类型。
  业务代码应该只看到 common 自己的接口。
- 不因为"仓库里已经拉下来了"就把接口直接暴露出去。

依赖现状（`deps/common/CMakeLists.txt`）：

| 库 | 链接方式 | 使用位置 | 状态 |
| --- | --- | --- | --- |
| rapidjson | `PRIVATE` | 仅 `config/config_json.cpp` | 符合规范，可作为样板 |
| spdlog | `PUBLIC` | `log/logging.h` | 头文件暴露了 `spdlog::logger`，待整改 |
| boost | `PUBLIC` | `base/timestamp.h` | 只用了 `boost/operators.hpp` 一个头，待移除 |

## 5. 编码规范

- **命名**：类型 `PascalCase`；函数、变量、成员函数 `snake_case`；私有成员变量
  尾部下划线；常量 `kCamelCase`；头文件名小写下划线，且与主要类型同名
  （`config.h` / `config_value.h`）。
- **命名空间**：统一用 `common::<模块>`（`common::log`、`common::base`、
  `common::config`、`common::util`）。现存仍在全局命名空间的类型
  （`Config`、`Thread`、`Timestamp`）在有第二个调用方时迁入。
- **头文件自包含**：每个头文件单独编译通过（`#include` 它不需要先包含别的头）。
- **包含顺序**：自身头文件最前，其余由 clang-format（Google 风格，80 列）自动
  分组排序，不手工干预。
- **注释**：只写"为什么"，不写"是什么"。算法、约束、坑要写清楚；显而易见的代码
  不加注释。
- **错误处理**：失败一律抛异常，不用"返回码 + 出参"。异常信息必须带定位信息
  （文件路径、配置字段路径等）。异常类型按语义区分：配置类错误用 `ConfigError`
  这类业务异常，系统调用失败用 `std::system_error`。
- **资源管理**：一律 RAII，禁止裸 `new`/`delete`、裸 `close`/`fclose`。
- **参数类型**：路径、文件名这类"要交给 C 接口的文本"用 `const std::string&`——
  `std::string` 保证 NUL 结尾，可以直接给 `open(2)`/`stat(2)`，不用在实现里
  再物化一份。只读的内容片段用 `std::string_view`（如 `config` 解析出来的
  配置内容）。同一个函数不要同时提供两种重载：字面量调用会因转换歧义编译
  失败。
- **线程安全**：默认**不**线程安全；确为线程安全的要在接口注释里写明。
- **禁止静默失败**：解析失败、类型不匹配等都要报错，不能悄悄用默认值兜底。

## 6. 测试规范

- 测试放在 `deps/common/tests/`，文件名 `<模块>_test.cpp`（如 `config_test.cpp`）。
- CMake 登记方式见 `deps/common/CMakeLists.txt`：`add_headers_and_sources()`
  收集源码，`_add_gtest()` 注册用例。
- 用例要覆盖**正常路径 + 错误路径**，错误路径要断言错误信息内容（不只是断言抛异常）。
- 用例不得依赖运行时工作目录：数据文件路径通过 `COMPILE_DEFINITIONS` 传入绝对
  路径（参考 `config_test` 的 `MOBAN_TEST_DATA_DIR`）。
- 临时文件、临时目录用 RAII 辅助对象自动清理，不留残留。
- 提交前 `ctest` 全绿。

## 7. 提交前检查清单

- [ ] `clang-format` 已应用（`.clang-format`）
- [ ] `clang-tidy` 无新增告警（`.clang-tidy`）
- [ ] 编译无警告
- [ ] `ctest` 全部通过
- [ ] 开启 sanitizer 的构建通过（`-DSANITIZE=address,undefined`）
- [ ] 新增公共接口有对应单测
- [ ] 没有引入违反第 2、3 节的内容

CI（`.github/workflows/build.yml`）在 push/PR 到 `master` 时执行
`make build-tests && make check-tests`。

## 8. 当前状态与技术债

| 项 | 现状 | 处理 |
| --- | --- | --- |
| 命名空间 | `Config`、`Thread`、`Timestamp` 等在全局 | 逐步迁入 `common::` |
| 目标划分 | 单一 `lib_common` 静态库 | 按模块拆成独立 target，让业务只链接需要的部分 |
| 分发方式 | 无 `install`/`export`，只能在 MOBAN 内使用 | 补 install rules + 导出配置，使 common 可被其他仓库 `find_package` 消费 |
| boost 依赖 | `timestamp.h` 为 `boost::totally_ordered1` 引入整个 boost | 手写 `<=`/`>`/`>=` 后移除该依赖 |
| spdlog 暴露 | `logging.h` 直接暴露 `spdlog::logger` | 后续用自有类型封装 |
| 测试覆盖 | `base/`、`utilities/` 有用例，`config/` 之外的模块基本没有 | 随新组件补齐 |

## 9. 待补齐清单

按"用得频率"和"其他模块对它的依赖"排序。新增模块请同步更新本表。

| 优先级 | 模块 | 内容 |
| --- | --- | --- |
| P0 | `utilities/string_utils` | trim、split、join、大小写转换、前后缀判断、替换、安全数字转换（带错误检查）——**已完成**，命名空间 `common::util` |
| P0 | `utilities/file_utils` | 文件读写、存在性/类型/大小判断、目录列举、原子写（临时文件 + rename）——**已完成**，命名空间 `common::util`，路径参数用 `const std::string&` |
| P0 | `utilities/path_utils` | 路径拼接、规范化（消解 `.`/`..`）、扩展名与分隔符处理 |
| P0 | `utilities/temp_file` | 临时文件 / 临时目录 RAII，供测试与运行时使用 |
| P1 | `base/error` | errno → 异常的系统调用封装、`Result`/`Status` 风格返回类型 |
| P1 | `utilities/unique_fd` | 文件描述符、`FILE*` 等句柄的 RAII 封装 |
| P1 | `utilities/encoding` | base64、hex 编解码 |
| P1 | `utilities/digest` | md5、sha256、crc32 |
| P2 | `base/sync` | Mutex / RWLock / ConditionVariable / SpinLock 薄封装、`ThreadLocal` |
| P2 | `base/timer` | 轻量定时器与超时（不依赖任何网络库） |
| P2 | `log/context` | trace id 注入日志、采样 |
| P3 | `metrics` | counter / gauge / histogram，供服务暴露指标 |
| P3 | `tests/` 辅助库 | 随机端口、日志捕获、基准测试 |

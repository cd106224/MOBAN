# MOBAN CMake

`cmake/` 存放 MOBAN 的构建脚本片段。根 `CMakeLists.txt` 只做"组装"：探测环境、
引入片段、设置全局变量、再把 `deps/third_party`、`deps/common`、`plugins`、
`example` 挂进来。所有跨模块共享的构建逻辑都收敛在这个目录里，根
`CMakeLists.txt` 不放具体逻辑。

这些片段只服务于**本仓库的构建**，不导出、不安装，不追求通用性。

---

## 1. 文件一览

| 文件 | 职责 | 引入位置 |
| --- | --- | --- |
| `ccache.cmake` | 编译缓存（`COMPILER_LAUNCHER`） | 根 `CMakeLists.txt` 第 5 行 |
| `configure_gpu_platform.cmake` | 选择 CUDA / HIP 平台，配置编译器与架构 | 根第 8 行（须早于 `enable_language`） |
| `detect_architectures.cmake` | 运行时探测本机 GPU 计算能力，转成 `CMAKE_CUDA_ARCHITECTURES` | 由 `configure_gpu_platform.cmake` 引入 |
| `find_hip.cmake` | 定位 HIP 头文件与运行时库 | 由 `configure_gpu_platform.cmake` 引入 |
| `clang_format.cmake` | `format` / `check-format` 目标 | 根第 11 行 |
| `check_unimplemented.cmake` | `check-unimplemented` 目标 | 根第 12 行 |
| `arch.cmake` | 架构识别，导出 `ARCH_*` 变量；开启警告即错误 | 根第 13 行 |
| `find_thread.cmake` | `find_package(Threads REQUIRED)`，为第三方库提供 `Threads::Threads` | 根第 14 行 |
| `glob_source.cmake` | 源码收集宏 | 根第 15 行 |
| `sanitize.cmake` | sanitizer 开关 | 根第 16 行 |
| `gtest_helpers.cmake` | 测试注册，`build-tests` / `check-tests` 目标 | 根第 17 行 |
| `verion.cmake` | 生成版本号 `MOBAN_BACKEND_VERSION` | 根第 18 行 |
| `config_ptds.cmake` | per-thread default stream 配置函数（**当前未被引入**） | — |

`verion.cmake` 文件名里的 `verion` 是历史拼写错误（应为 `version`），改名会牵动引用，暂留。

## 2. 引入顺序

顺序不是随意的，几处是硬约束：

```text
project()
  └─ ccache.cmake            ← 必须在 project() 之后：project() 会初始化各语言的
                              COMPILER_LAUNCHER 并覆盖此处的设置
  └─ configure_gpu_platform.cmake
                              ← 必须在 enable_language() 之前：CUDA/HIP 的
                                COMPILER_LAUNCHER 要在语言启用前设好
  └─ enable_language(${PLATFORM})
  └─ clang_format / check_unimplemented / arch / find_thread
  └─ glob_source.cmake       ← 必须在 deps/、plugins/、example/ 之前
  └─ sanitize / gtest_helpers / verion
  └─ add_subdirectory(deps/third_party)   ← 依赖 arch.cmake 导出的 ARCH_* 变量
     add_subdirectory(deps/common)
     add_subdirectory(plugins)
     add_subdirectory(example)
```

`arch.cmake` 必须排在 `deps/third_party` 之前：`openssl-cmake`、`boost-cmake`、
`zlib-ng-cmake` 都用 `ARCH_AMD64` / `ARCH_AARCH64` / `ARCH_PPC64LE` /
`ARCH_S390X` / `ARCH_RISCV64` / `ARCH_LOONGARCH64` 里的**一个**来选架构专用
源码和编译选项。变量缺失时第三方库会走错分支或直接报错。

## 3. 构建选项

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `USE_CCACHE` | `ON` | 启用编译缓存。找不到 `ccache` 时只告警并自动降级为关闭，不中断配置 |
| `CCACHE_PROGRAM` | 自动查找 | 指定 ccache 可执行文件路径 |
| `CCACHE_DIR` | `$HOME/.cache/ccache` | 缓存目录 |
| `CCACHE_COMPILERCHECK` | `content` | 编译器变更检测方式（`mtime` / `content`）。用 `content` 比 `mtime` 慢一点，但不会因文件时间戳被保留而命中过期缓存 |
| `SANITIZE` | 空 | 逗号分隔，可选 `address` / `undefined`，如 `-DSANITIZE=address,undefined` |
| `PLATFORM` | `CUDA`；环境变量 `HIP_PLATFORM` 非空时为 `HIP` | GPU 平台，只能是 `CUDA` 或 `HIP`，其他值直接 `FATAL_ERROR` |
| `CMAKE_CUDA_ARCHITECTURES` | 自动探测 | 不指定时探测本机 GPU 的计算能力；指定则跳过探测 |
| `ENABLE_PLUGINS` | `ON` | 是否构建 `plugins/`（Muduo） |
| `ENABLE_EXAMPLE` | `ON` | 是否构建 `example/`（miniftpd） |
| `CMAKE_BUILD_TYPE` | 未设置 | 构建类型，CI 用 Ninja 默认配置 |

`PLATFORM` 的判定顺序：显式 `-DPLATFORM=` 优先；否则看环境变量 `HIP_PLATFORM`
是否非空；都没有则用 `CUDA`。

## 4. 提供的宏与函数

### `glob_source.cmake`

| 宏 | 说明 |
| --- | --- |
| `add_glob(<list> <glob...>)` | 收集文件追加到 `<list>`，路径相对当前源码目录。用 `CONFIGURE_DEPENDS`，新增文件会自动触发重新配置 |
| `add_headers_and_sources(<prefix> <dir>)` | 收集 `<dir>` 下的 `*.h` 到 `<prefix>_headers`，`*.cpp/*.c/*.cu` 到 `<prefix>_sources`。HIP 平台下把 `.cu` 的语言属性改写为 `HIP` |
| `add_headers_only(<prefix> <dir>)` | 只收集头文件（当前未被使用） |
| `extract_into_parent_list(<src_list> <dest_list> <file...>)` | 从 `<src_list>` 移除指定文件，改为以 `<当前目录名>/<file>` 的形式追加到父作用域的 `<dest_list>`（当前未被使用） |

用法（`deps/common/CMakeLists.txt`）：

```cmake
add_headers_and_sources(common base)
add_library(lib_common STATIC ${common_header} ${common_sources})
```

### `gtest_helpers.cmake`

`_add_gtest(<sources> [LINK_LIBS ...] [COMPILE_DEFINITIONS ...])` 为每个 `.cpp`
生成一个独立可执行目标并注册到 CTest：

- 目标名 = 去掉 `.cpp` 后缀的文件名。
- 默认链接 `_gtest_main _gtest pthread`；`LINK_LIBS` 一旦指定就**完全替换**默认值，不追加。
- `COMPILE_DEFINITIONS` 传绝对路径给测试用，例如
  `COMPILE_DEFINITIONS MOBAN_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/config_test/data"`。
- 可执行文件输出到 `${CMAKE_BINARY_DIR}/test/`，XML 报告同级。
- 用例超时 120 秒，`--gtest_catch_exceptions=0`（gtest 捕获异常后以失败退出，不吞异常）。

`cmake_parse_arguments` 的坑：未在 `multi_value_args` 里登记的关键字不会报错，而是被吞进紧邻的多值参数。新增可选参数必须同步登记到 `gtest_helpers.cmake:13`。

### `config_ptds.cmake`

`configure_ptds(<target> <platform>)`：给目标加 `DT_USE_PTDS`，CUDA 下用
`CUDA_API_PER_THREAD_DEFAULT_STREAM` 宏，HIP 下用 `-fgpu-default-stream=per-thread`。
**当前没有任何地方引入这个文件**，属于预留能力。

## 5. 构建目标

| 目标 | 作用 |
| --- | --- |
| `format` | 对 `deps/common`、`plugins`、`example` 跑 clang-format 并就地修改 |
| `check-format` | 只检查不修改，CI 用 |
| `check-unimplemented` | 扫 `deps/common`、`tests`、`plugins`、`example`，找"头文件里声明了但没有实现"的成员函数 |
| `build-tests` | 编译全部测试目标（其 `COMMAND` 是 `ctest --show-only`，只列用例不执行） |
| `check-tests` | 编译并运行全部测试（`ctest --verbose`） |

`format` / `check-format` / `check-unimplemented` 依赖 `build_support/` 下的两个
Python 脚本，找不到 `clang-format` 或 `python3` 时只告警并跳过该目标。

`check-unimplemented` 的扫描范围**不含** `deps/third_party`——第三方库里有大量
故意只声明不实现的接口，扫了全是噪声。

## 6. 已知约束与坑

- **ccache 与 nvcc**：ccache 3.x 能按 `.cu` 后缀缓存 nvcc，但认不出 nvcc 的
  `-x cu` 选项（CMake 编译 `.cu` 一定带它），命中会退化成 "unsupported source
  language" 直接透传；ccache 4.x 才补上支持。HIP 的 `hipcc` 一律不被 ccache
  支持。CI 用的是较新的 ccache，不受影响。
- **`-Werror`**：`arch.cmake` 对 GNU/Clang 加 `-Werror`，MSVC 加 `/WX`。第三方库
  通过 `global-group`（根 `CMakeLists.txt` 创建的 INTERFACE 库）关掉个别告警，
  例如 openssl 的 `-Wno-deprecated-declarations`。**升级编译器时第三方库的新告警
  会直接让构建失败**，这是有意为之，但排查起来要看具体是哪个 target。
- **sanitizer 语义**：`undefined` 额外带 `-fno-sanitize-recover=all`。UBSan 默认
  报错后继续执行并返回 0，在 CI 里容易被当成"通过"而漏掉，这里改成致命错误。
- **CUDA 宿主编译器**：`configure_gpu_platform.cmake` 在 `CMAKE_CUDA_HOST_COMPILER`
  未指定时找 `g++-9`/`g++-10`/`g++-11`——CUDA 11.5 的 nvcc 只支持到 GCC 11。
  普通 C++ 代码仍用系统默认的 `CMAKE_CXX_COMPILER`，两者是分开的。
- **架构探测失败会静默回退**：`detect_architectures.cmake` 探测不到 GPU 时退回
  传入的默认值并只打 `STATUS` 日志，不报错。

## 7. 当前状态与技术债

| 项 | 现状 | 处理 |
| --- | --- | --- |
| `config_ptds.cmake` | 定义了 `configure_ptds` 但无人引入 | 引入 GPU 流配置时启用，或删掉 |
| `add_headers_only` / `extract_into_parent_list` | 定义后无人使用 | 确认无使用后删除 |
| `find_thread.cmake` | 只有一行 `find_package(Threads REQUIRED)`，但不能删 | `abseil-cmake` 直接使用 `Threads::Threads` 而自己不调 `find_package(Threads)`，依赖根目录这次全局查找 |
| `verion.cmake` | 文件名拼写错误；`execute_process` 查 `which git`，依赖系统 `which` 而非 `find_program` | 保持不动，避免牵动引用 |
| `configure_gpu_platform.cmake` | 用 `$ENV{HIP_PLATFORM}` 直接展开，虽未定义变量也未报错，但可读性差 | 改成 `if (NOT "$ENV{HIP_PLATFORM}" STREQUAL "")` |
| `clang_format.cmake` | 硬编码搜索路径 `/usr/local/bin` 等 | 交给 `find_program` 的默认路径 |
| `check-unimplemented` 扫描范围 | 包含根目录 `tests/`，但该目录目前是空的 | 目录启用时补上对应内容，或从 `CHECK_DIRS` 移除 |
| 平台支持 | 只维护 Linux；`deps/common` 是跨平台组件库，`_WIN32` 分支需要保留 | 见 `deps/common/README.md` |

## 8. 常用命令

```bash
# 配置（自动探测 GPU 架构）
cmake -S . -B build -G Ninja

# 指定平台与 GPU 架构
cmake -S . -B build -G Ninja -DPLATFORM=CUDA -DCMAKE_CUDA_ARCHITECTURES=75

# 开启 sanitizer
cmake -S . -B build -G Ninja -DSANITIZE=address,undefined

# 编译 + 跑测试
cmake --build build --target build-tests
ctest --test-dir build --output-on-failure

# 格式化与检查
cmake --build build --target format
cmake --build build --target check-format
cmake --build build --target check-unimplemented
```

CI（`.github/workflows/build.yml`）在 push/PR 到 `master` 时执行
`build-tests` + `ctest`，跑在 `nvidia/cuda:12.5.1-devel-ubuntu22.04` 容器里，
固定 CMake 3.31.12（镜像自带的 3.22 低于项目要求的 3.26）。

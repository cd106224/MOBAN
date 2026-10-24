# MOBAN di 层

`di` = device independent，设备无关的加速器接口抽象层。目标是把"业务代码"和
"具体加速器后端"隔开：同一份业务代码在 CUDA 和 HIP 上都能编，且语义一致。

规划三路后端：

| 后端 | 目录 | 状态 |
| --- | --- | --- |
| CUDA | `cuda/` | 已实现并验证 |
| HIP | `hip/` | 已实现，**未在真实环境验证** |
| host | `host/` | 未接入（见第 10 节） |

> 本文件只完整记录 **CUDA 后端**。HIP 与 host 的章节留空，等有对应环境后补。

---

## 1. 定位与边界

这一层只做一件事：**把各后端的运行时 API 收敛成一套 `dev*` 命名的接口**。

- 它**不含**任何加速算法。排序、归约、join 这类算子不属于这里。
- 它**不含**任何业务语义。一个"算子"要不要用 GPU，不在这一层决定。
- 它**不做**内存池、缓存、同步策略这类资源管理设计——那一层在 di 之上。

与 [deps/common/README.md](../README.md) 的关系：di 是 common 里**唯一**允许直接
依赖硬件 SDK 的模块，其余模块不得 include di 的任何头。

---

## 2. 目录结构

```text
di/
├── ddi_common.h              平台判定, 定义 __PLATFORM_NVCC__ / __PLATFORM_HIPCC__
├── ddi_runtime.h             运行时 API 的分发入口
├── ddi_cooperative_groups.h  cooperative groups 的分发入口
├── ddi_vector_types.h        向量类型的分发入口
├── cub.h                     cub / hipcub 的分发入口
├── cuda/                     CUDA 后端
│   ├── cuda_runtime.h        聚合入口
│   ├── cuda_runtime_api.h    函数映射
│   └── driver_types.h        类型与常量映射
├── hip/                      HIP 后端
└── host/                     无设备后端
```

**分发是单向的**：上层 `ddi_*.h` 只负责按平台宏选一个后端头文件进来，不含任何
后端差异的处理逻辑；差异全部沉在 `cuda/`、`hip/` 各自的实现里。

---

## 3. 平台判定

[ddi_common.h](ddi_common.h) 依据编译器宏推导平台：

```c
#if defined(__clang__) && defined(__HIP__)
#define __PLATFORM_HIPCC__
#endif

#if defined(__NVCC__) || \
    (defined(__clang__) && defined(__CUDA__) && !defined(__HIP__))
#define __PLATFORM_NVCC__
#endif
```

各 `ddi_*.h` 在分发时要求**恰好命中一个**平台，否则 `#error`：

```c
#if defined(__PLATFORM_HIPCC__) && !defined(__PLATFORM_NVCC__)
#include "di/hip/hip_runtime.h"
#elif defined(__PLATFORM_NVCC__) && !defined(__PLATFORM_HIPCC__)
#include "di/cuda/cuda_runtime.h"
#else
#error("Must define exactly one of __PLATFORM_HIPCC__ or __PLATFORM_NVCC__");
#endif
```

注意这两个宏的命名有误导性：它们判的是**平台**，不是编译器（HIPCC 只是 CUDA
侧对应的另一个平台）。

> 纯主机编译器（不带任何 GPU）编译时，两个宏都不满足，包含这些头的编译单元
> 会直接 `#error`。这是有意的：di 的使用者必须显式选择后端。

---

## 4. 命名约定

| 前缀 | 含义 | 示例 |
| --- | --- | --- |
| `dev` + 大驼峰 | 函数 | `devMalloc` → `cudaMalloc` |
| `dev` + 大驼峰 + `_t` | 类型 | `devStream_t` → `cudaStream_t` |
| `dev` + 大驼峰（大驼峰常量） | 常量/枚举 | `devSuccess` → `cudaSuccess` |

业务代码**只能**用 `dev*` 名字，不得直接出现 `cuda*` / `hip*`，否则该代码换后端
时必然要改。

---

## 5. CUDA 后端映射清单

### 5.1 函数

| 分类 | di 接口 | CUDA 接口 |
| --- | --- | --- |
| 设备管理 | `devGetDevice` | `cudaGetDevice` |
| | `devSetDevice` | `cudaSetDevice` |
| | `devGetDeviceCount` | `cudaGetDeviceCount` |
| | `devDeviceGetAttribute` | `cudaDeviceGetAttribute` |
| | `devGetDeviceProperties` | `cudaGetDeviceProperties` |
| | `devDeviceReset` | `cudaDeviceReset` |
| | `devDeviceSynchronize` | `cudaDeviceSynchronize` |
| 错误处理 | `devGetLastError` | `cudaGetLastError` |
| | `devPeekAtLastError` | `cudaPeekAtLastError` |
| | `devGetErrorName` | `cudaGetErrorName` |
| | `devGetErrorString` | `cudaGetErrorString` |
| 流 | `devStreamCreate` | `cudaStreamCreate` |
| | `devStreamDestroy` | `cudaStreamDestroy` |
| | `devStreamSynchronize` | `cudaStreamSynchronize` |
| | `devStreamWaitEvent` | `cudaStreamWaitEvent` |
| 事件 | `devEventCreate` | `cudaEventCreate` |
| | `devEventCreateWithFlags` | `cudaEventCreateWithFlags` |
| | `devEventDestroy` | `cudaEventDestroy` |
| | `devEventRecord` | `cudaEventRecord` |
| | `devEventSynchronize` | `cudaEventSynchronize` |
| | `devEventElapsedTime` | `cudaEventElapsedTime` |
| 内存 | `devMalloc` | `cudaMalloc` |
| | `devMallocAsync` | `cudaMallocAsync`（需 CUDA ≥ 11.2） |
| | `devMallocHost` | `cudaMallocHost` |
| | `devMallocManaged` | `cudaMallocManaged` |
| | `devHostAlloc` | `cudaHostAlloc` |
| | `devFree` | `cudaFree` |
| | `devFreeHost` | `cudaFreeHost` |
| | `devMemset` | `cudaMemset` |
| | `devMemsetAsync` | `cudaMemsetAsync` |
| | `devMemcpy` | `cudaMemcpy` |
| | `devMemcpyAsync` | `cudaMemcpyAsync` |
| | `devMemGetInfo` | `cudaMemGetInfo` |
| | `devPointerGetAttributes` | `cudaPointerGetAttributes` |
| 符号 | `devGetSymbolAddress` | `cudaGetSymbolAddress` |
| | `devMemcpyToSymbol` | `cudaMemcpyToSymbol` |
| | `devMemcpyToSymbolAsync` | `cudaMemcpyToSymbolAsync` |
| | `devMemcpyFromSymbol` | `cudaMemcpyFromSymbol` |
| | `devMemcpyFromSymbolAsync` | `cudaMemcpyFromSymbolAsync` |
| P2P | `devDeviceCanAccessPeer` | `cudaDeviceCanAccessPeer` |
| | `devDeviceEnablePeerAccess` | `cudaDeviceEnablePeerAccess` |
| | `devDeviceDisablePeerAccess` | `cudaDeviceDisablePeerAccess` |
| | `devMemcpyPeer` | `cudaMemcpyPeer` |
| | `devMemcpyPeerAsync` | `cudaMemcpyPeerAsync` |
| kernel 启动 | `devLaunchKernelGGL` | 见 5.4 |

### 5.2 类型

| di 类型 | CUDA 类型 |
| --- | --- |
| `devError_t` | `cudaError_t` |
| `devStream_t` | `cudaStream_t` |
| `devEvent_t` | `cudaEvent_t` |
| `devPointerAttribute_t` | `cudaPointerAttributes` |
| `devDeviceProp_t` | `cudaDeviceProp` |

### 5.3 常量

| di 常量 | CUDA 常量 | 备注 |
| --- | --- | --- |
| `devSuccess` | `cudaSuccess` | |
| `devErrorMemoryAllocation` | `cudaErrorMemoryAllocation` | |
| `devStreamDefault` / `NonBlocking` / `Legacy` / `PerThread` | 同名 | |
| `devEventDefault` / `BlockingSync` / `DisableTiming` / `Interprocess` | 同名 | |
| `devMemcpyHostToHost` … `devMemcpyDefault` | 同名 | |
| `devMemoryTypeHost` / `Device` / `Managed` | 同名 | **两后端枚举值不同，见 7.4** |
| `devHostAllocPortable` | `cudaHostAllocPortable` | |
| `devDevAttrMaxSharedMemoryPerBlock` | `cudaDevAttrMaxSharedMemoryPerBlock` | |

### 5.4 kernel 启动宏

```c
#define devLaunchKernelGGL(kernelName, numBlocks, numThreads, memPerBlock, streamId, ...) \
    do {                                                                                    \
        kernelName<<<(numBlocks), (numThreads), (memPerBlock), (streamId)>>>(__VA_ARGS__);  \
    } while (0)
```

参数顺序与 CUDA 的 `<<<>>>` 语法一致。HIP 侧展开为 `hipLaunchKernelGGL`，需要额外
包一层 `HIP_KERNEL_NAME` 和 `dim3`——**这正是宏映射无法表达后端差异的一个例子**，
两边的展开结果并不等价。

---

## 6. 映射规则

**枚举一律映射到符号名，不映射到数值。** 例如 `devMemcpyHostToDevice` 映射到
`cudaMemcpyHostToDevice` 而不是 `1`。因为各后端的枚举数值不保证一致（见 7.4），
按数值映射等于把后端差异藏起来。

**带版本门槛的 API 用条件编译暴露。** CUDA 侧：

```c
#if CUDART_VERSION >= 11020  // 11.2 introduced cudaMallocAsync
#define devMallocAsync cudaMallocAsync
#endif
```

HIP 侧用 `HIP_VERSION >= 502` 做同样的事。低于该版本时 `devMallocAsync` 这个
符号**不存在**，引用它的代码在旧环境下直接编译失败——这是刻意的：与其运行时
才发现不支持，不如编译期报错。

---

## 7. CUDA 侧的已知约束

以下都是在本仓库用 CUDA 11.5 实测确认的。**前两条是坑，后几条是容易记错的
API 语义。**

### 7.1 符号参数必须传变量名，不能传地址

这是 CUDA 侧最容易踩、也最难查的一条。

在 host 代码里，`__device__` 变量的**地址不是设备地址**。把它传给
`cudaMemcpyToSymbol` / `cudaMemcpyFromSymbol` / `cudaGetSymbolAddress` 会得到
`cudaErrorInvalidSymbol`（错误码 13），而且**编译期完全正常**。

实测结果：

| 调用 | 传 `&gValue` | 传 `gValue` |
| --- | --- | --- |
| `cudaMemcpyToSymbol` | 13 `cudaErrorInvalidSymbol` | 0 成功 |
| `cudaMemcpyFromSymbol` | 13 | 0 成功 |
| `cudaGetSymbolAddress` | 13 | 0 成功，拿到设备地址 |

正确写法：

```cpp
__device__ int gValue = 0;

void* addr = nullptr;
cudaGetSymbolAddress(&addr, gValue);          // 注意: 不带 &
cudaMemcpyToSymbol(gValue, &host_value, sizeof(int), 0, cudaMemcpyHostToDevice);
```

> **与 HIP 侧疑似正好相反。** HIP 的 `hipMemcpyToSymbol` 是 `T*` 模板重载，按
> 模板语义应当**传地址**——若确实如此，同一份业务代码在两个后端上就不可能都
> 写对。这组 API 后续需要改成包装函数，见第 9 节。HIP 侧的确切行为需在真实
> ROCm 环境核对，见第 10 节。

### 7.2 cooperative groups 只能在 device 代码里用

`cg::this_thread_block()`、`cg::tiled_partition<...>()`、tile 的 `thread_rank()`
全是 `__device__` 函数。在 host 函数里调用会直接编译失败：

```text
error: calling a __device__ function("cooperative_groups::__v1::this_thread_block()")
       from a __host__ function is not allowed
```

要做编译期检查就把代码放进 `__global__` 函数里——`__global__` 只要被编译就足以
触发全部类型检查，不需要真的启动。

另外，CUDA 的 `cg::thread_block_tile<32>` 对应一个 **warp（32 线程）**，而 AMD
的 wavefront 是 **64 线程**。同一个模板参数在两个后端上不等价，这类差异不会
报错、只会静默地给出不同的分组边界。

### 7.3 CUDA 11 改过一批签名

| API | 现在的签名 |
| --- | --- |
| `cudaGetErrorName` | `const char* cudaGetErrorName(cudaError_t)` |
| `cudaGetErrorString` | `const char* cudaGetErrorString(cudaError_t)` |
| `cudaDeviceEnablePeerAccess` | `cudaError_t cudaDeviceEnablePeerAccess(int peerDevice, unsigned int flags)` |
| `cudaDeviceDisablePeerAccess` | `cudaError_t cudaDeviceDisablePeerAccess(int peerDevice)` |

后两个的 `device` 取**当前设备**，不再显式传参。HIP 侧仍保持旧的三参数签名。

### 7.4 `cudaMemoryType` 的枚举值两后端不同

`cudaMemoryType` 是 `Unregistered=0, Host=1, Device=2, Managed=3`；HIP 的
`hipMemoryType` 取值不同（di 层自己的注释也记了这一点，但具体数值需在 ROCm
环境核对）。di 层用同一个 `devMemoryTypeHost` 映射到两边，**任何把这些值当整数
存下来或做比较的代码，换后端就是错的**。

### 7.5 last error 是粘滞的

`cudaGetLastError()` 返回**上一次**遗留的错误并清空它。一次失败的调用会把错误
留到后面，用例之间不清理就会串味。写测试时应在每条用例开始时先读一次。

### 7.6 其它容易记错的语义

- `cudaEventElapsedTime(float* ms, cudaEvent_t start, cudaEvent_t end)` 比的是
  **两个事件**，不是"事件 + 流"
- `cudaMemset` 按**字节**填：填 `0x5A` 读出来是 `0x5A5A5A5A`

---

## 8. 新增一个 API 的步骤

1. 在 `cuda/cuda_runtime_api.h`（函数）或 `cuda/driver_types.h`（类型/常量）里
   加映射，**用符号名映射，不要用数值**
2. 若该 API 在不同 CUDA 版本间有出入，加版本条件编译
3. 在 HIP 后端加对应映射；若两边语义不同，先在第 9 节登记
4. 在 `tests/di_compile_test.cu` 里加一次调用——编译期检查
5. 在 `tests/di_smoke_test.cu` 里加一条用例——运行期检查
6. 跑 `cmake --build build --target build-tests && ctest --test-dir build`

---

## 9. 已知问题

**三组 API 在 CUDA 与 HIP 上签名或语义不同，宏映射表达不了，需要改成包装函数：**

| 组 | 差异 |
| --- | --- |
| `devGetErrorName` / `devGetErrorString` | CUDA 11 返回 `const char*`，HIP 是 `(err, char**)` |
| `devDeviceEnablePeerAccess` / `devDeviceDisablePeerAccess` | CUDA 11 是 `(peerDevice, flags)`，HIP 是 `(device, peerDevice, flags)` |
| `devMemcpyToSymbol` / `devMemcpyFromSymbol` / `devGetSymbolAddress` | CUDA 传变量名，HIP 传地址（见 7.1） |

改成包装函数（每个后端一个 inline 实现）后，业务代码就能用同一份写法。

**API 覆盖缺口**（按重要性）：

- **stream-ordered allocator**：`devMallocAsync` 已有，但缺 `devFreeAsync` /
  `devMemPoolCreate` / `devMemPoolDestroy` / `devMallocFromPoolAsync`。没有这一组
  就撑不起真正的 GPU 内存池抽象
- **warp intrinsics**：`__shfl_sync` / `__ballot_sync` / `__activemask` /
  `__popc` 一个都没有，而它们是 warp32 与 wave64 差异的核心
- occupancy 相关：`devFuncGetAttributes` /
  `devOccupancyMaxActiveBlocksPerMultiprocessor`
- `devStreamQuery` / `devEventQuery`
- `devMemAdvise` / `devMemPrefetchAsync`（managed memory）

---

## 10. HIP 后端

待补。当前状态：代码已写，但**从未在真实 ROCm 环境编译或运行过**。

已知待确认项（均需真实 HIP 头文件核对，不能凭记忆推断）：

- `HIP_SYMBOL(...)` 套在 `__device__` 变量上是否成立
- `HIP_KERNEL_NAME(...)` 是否存在、语义为何
- `hipGetErrorName` / `hipGetErrorString` 的确切签名
- `hipDeviceEnablePeerAccess` / `hipDeviceDisablePeerAccess` 的确切签名
- `hipMemoryType` 枚举值
- `hipMemcpyToSymbol` 模板重载与 `HIP_SYMBOL` 的组合行为

## 11. host 后端

待补。当前状态：**不可达，且自身不完整**。

- `ddi_*.h` 的分发只有 HIP / CUDA 两个分支，没有 host 分支，因此 [host/](host/)
  下的内容目前不会被任何编译单元包含
- [host/host_defines.h](host/host_defines.h) include 的是 CUDA 的**内部头**
  `host_defines.h`，CUDA 自己会警告它"不应直接使用且未来会移除"。也就是说这个
  面向"无 GPU 环境"的后端，没有 CUDA 反而编译不过
- API 覆盖比 CUDA 少：缺 `devGetDeviceProperties` / `devDeviceProp_t`

设计意图是"编译期桩 + 运行期硬失败"（`__RuntimeError` 打印文件行号后 abort），
而不是 CPU 模拟。是否接入，取决于是否真的需要"在无 GPU 的机器上编译整套代码"
这个场景。

// di 层的编译期映射检查。
//
// di/ 的价值在于"同一份业务代码在 CUDA 和 HIP 下都能编、且语义一致"。
// 这里只做编译期检查, 不运行: 任何"宏映射写错 / 参数个数对不上 / 类型对不上"
// 的问题都会在编译阶段暴露, 不需要 GPU, CI 里也能跑。
//
// 下面这些函数永远不会被调用, 存在的意义只是让编译器把每个 API 的签名
// 完整走一遍。用 [[maybe_unused]] 是因为全局开了 -Werror, 而未使用的
// 内部函数会触发 -Wunused-function。
//
// 已知的两处跨后端签名差异(截至 CUDA 11.5 / HIP 5.x):
//
//  1. 错误名/错误文本
//     CUDA 11 起 cudaGetErrorName(cudaError_t) 直接返回 const char*,
//     HIP 仍是 hipGetErrorName(hipError_t, char**) 的老签名。
//  2. P2P 开关
//     CUDA 11 的 cudaDeviceEnablePeerAccess(int peerDevice, unsigned int flags)
//     与 cudaDeviceDisablePeerAccess(int peerDevice) 里, device 取当前设备;
//     HIP 仍是 (device, peerDevice, flags) / (device, peerDevice)。
//
// 这两组 di 层目前是纯宏映射, 没法用同一份调用写通, 需要改成包装函数。
// 在有 ROCm 环境之前, 这里只覆盖 CUDA 分支。

#include "di/cub.h"
#include "di/ddi_cooperative_groups.h"
#include "di/ddi_runtime.h"
#include "di/ddi_vector_types.h"

namespace {

__device__ int gDeviceValue = 0;

__global__ void DummyKernel(int* out) { *out = 42; }

/* cg 的 API 全是 __device__ 函数, 只能写在 device 代码里, 放在 host 函数中
 * 调用会直接编译失败。所以这里放进 __global__ —— __global__ 函数只要被编译
 * 就足以完成映射检查, 不需要真的启动。
 *
 * CUDA 的 thread_block_tile<32> 是 warp, AMD 的 wavefront 是 64,
 * 这里固定 32 是为了在两个后端上比对分组语义。 */
__global__ void CooperativeGroupsKernel(int* out) {
  namespace cg = cooperative_groups;
  cg::thread_block_tile<32> tile =
      cg::tiled_partition<32>(cg::this_thread_block());
  *out = static_cast<int>(tile.thread_rank());
}

/* 设备管理 */
[[maybe_unused]] void CheckDeviceManagement() {
  int device = 0;
  int count = 0;
  (void)devGetDevice(&device);
  (void)devSetDevice(device);
  (void)devGetDeviceCount(&count);
  (void)devDeviceReset();
  (void)devDeviceSynchronize();
}

/* 设备属性 */
[[maybe_unused]] void CheckDeviceProperties() {
  devDeviceProp_t prop{};
  (void)devGetDeviceProperties(&prop, 0);

  int max_shmem = 0;
  (void)devDeviceGetAttribute(&max_shmem, devDevAttrMaxSharedMemoryPerBlock, 0);
}

/* 内存管理 */
[[maybe_unused]] void CheckMemory() {
  void* ptr = nullptr;
  (void)devMalloc(&ptr, 1024);
  (void)devMallocHost(&ptr, 1024);
  (void)devMallocManaged(&ptr, 1024);
  (void)devHostAlloc(&ptr, 1024, devHostAllocPortable);

  (void)devMemset(ptr, 0, 1024);
  (void)devMemcpy(ptr, ptr, 1024, devMemcpyHostToDevice);
  (void)devMemcpyAsync(ptr, ptr, 1024, devMemcpyDeviceToHost, nullptr);

  size_t free_bytes = 0;
  size_t total_bytes = 0;
  (void)devMemGetInfo(&free_bytes, &total_bytes);

  devPointerAttribute_t attr{};
  (void)devPointerGetAttributes(&attr, ptr);

  (void)devFree(ptr);
  (void)devFreeHost(ptr);
}

/* 异步分配 */
#if defined(__PLATFORM_NVCC__) && CUDART_VERSION >= 11020
[[maybe_unused]] void CheckAsyncAllocation() {
  void* ptr = nullptr;
  (void)devMallocAsync(&ptr, 1024, nullptr);
  (void)devFree(ptr);
}
#endif

/* 流与事件 */
[[maybe_unused]] void CheckStreamAndEvent() {
  devStream_t stream = nullptr;
  devEvent_t start = nullptr;
  devEvent_t end = nullptr;

  (void)devStreamCreate(&stream);
  (void)devEventCreate(&start);
  (void)devEventCreateWithFlags(&end, devEventDisableTiming);
  (void)devEventRecord(start, stream);
  (void)devEventRecord(end, stream);
  (void)devStreamWaitEvent(stream, end, 0);
  (void)devEventSynchronize(start);
  (void)devEventSynchronize(end);

  // cudaEventElapsedTime 比的是两个事件, 不是"事件 + 流"
  float elapsed_ms = 0;
  (void)devEventElapsedTime(&elapsed_ms, start, end);

  (void)devStreamSynchronize(stream);
  (void)devStreamDestroy(stream);
  (void)devEventDestroy(start);
  (void)devEventDestroy(end);
}

/* 错误处理 */
[[maybe_unused]] void CheckErrorHandling() {
  (void)devGetLastError();
  (void)devPeekAtLastError();

#if defined(__PLATFORM_NVCC__)
  // CUDA 11 起直接返回 const char*, HIP 还是 (err, char**)。
  const char* name = devGetErrorName(devSuccess);
  const char* text = devGetErrorString(devSuccess);
  (void)name;
  (void)text;
#elif defined(__PLATFORM_HIPCC__)
  // TODO: 待有 ROCm 环境后按 HIP 头文件补齐。
#endif
}

/* 设备间拷贝与 P2P */
[[maybe_unused]] void CheckPeerAccess() {
  void* ptr = nullptr;
  (void)devMalloc(&ptr, 1024);

  (void)devMemcpyPeer(ptr, 0, ptr, 0, 1024);
  (void)devMemcpyPeerAsync(ptr, 0, ptr, 0, 1024, nullptr);

  int can_access = 0;
  (void)devDeviceCanAccessPeer(&can_access, 0, 0);

#if defined(__PLATFORM_NVCC__)
  // CUDA 11 的签名里 device 取当前设备, 只传 peerDevice。
  (void)devDeviceEnablePeerAccess(1, 0);
  (void)devDeviceDisablePeerAccess(1);
#elif defined(__PLATFORM_HIPCC__)
  // TODO: 待有 ROCm 环境后按 HIP 头文件补齐。
#endif

  (void)devFree(ptr);
}

/* 符号地址与符号拷贝
 *
 * CUDA 的符号参数必须传变量名本身: 传 &var 会返回 cudaErrorInvalidSymbol,
 * 因为 host 代码里 __device__ 变量的地址不是设备地址。
 * HIP 的 hipMemcpyToSymbol 是 T* 模板重载, 又必须传地址 —— 两边正好相反,
 * di 层用宏映射表达不了这组 API, 需要改成包装函数。
 * 这里只覆盖 CUDA 的写法, HIP 分支待包装函数落地后补。 */
#if defined(__PLATFORM_NVCC__)
[[maybe_unused]] void CheckSymbolCopy() {
  void* addr = nullptr;
  (void)devGetSymbolAddress(&addr, gDeviceValue);

  int host_value = 7;
  (void)devMemcpyToSymbol(gDeviceValue, &host_value, sizeof(int), 0,
                          devMemcpyHostToDevice);
  (void)devMemcpyFromSymbol(&host_value, gDeviceValue, sizeof(int), 0,
                            devMemcpyDeviceToHost);
}
#elif defined(__PLATFORM_HIPCC__)
// TODO: 待 di 层把这组 API 改成包装函数后补齐。
#endif

/* kernel 启动 */
[[maybe_unused]] void CheckKernelLaunch() {
  int* out = nullptr;
  devLaunchKernelGGL(DummyKernel, 1, 32, 0, nullptr, out);
  devLaunchKernelGGL(CooperativeGroupsKernel, 1, 32, 0, nullptr, out);
}

/* 向量类型 */
[[maybe_unused]] void CheckVectorTypes() {
  float2 v{};
  v.x = 1.0f;
  v.y = 2.0f;
  (void)v;
}

}  // namespace

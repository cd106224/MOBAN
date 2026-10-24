// di 层的运行期冒烟测试。
//
// di_compile_test.cu 只能保证"签名对得上", 这里验证"映射出来的调用真的能用":
// 分配、拷贝、符号读写、kernel 启动、流与事件、错误码。
//
// 同一份源码在 CUDA 和 HIP 下必须给出相同结果, 这正是 di 层存在的意义。
//
// 没有可用的加速器设备时整组测试 skip, 这样在无 GPU 的机器上不会变红。

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "di/ddi_cooperative_groups.h"
#include "di/ddi_runtime.h"

namespace {

constexpr int kTileSize = 32;

// memset 是按字节填充的, 所以 0x5A 填进去读出来是 0x5A5A5A5A
constexpr int kBytePattern = 0x5A;
constexpr int kFilledValue = 0x5A5A5A5A;

// 符号参数要传变量名本身, 不能传 &var
__device__ int gDeviceValue = 0;

__global__ void AddKernel(const int* lhs, const int* rhs, int* out, int count) {
  const int idx = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
  if (idx < count) {
    out[idx] = lhs[idx] + rhs[idx];
  }
}

__global__ void TileRankKernel(int* out) {
  namespace cg = cooperative_groups;
  cg::thread_block_tile<32> tile =
      cg::tiled_partition<32>(cg::this_thread_block());
  out[tile.thread_rank()] = 1;
}

class DiSmokeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int count = 0;
    if (devGetDeviceCount(&count) != devSuccess || count <= 0) {
      GTEST_SKIP() << "没有可用的加速器设备, 跳过 di 层运行期测试";
    }
    ASSERT_EQ(devSetDevice(0), devSuccess);

    // last error 是粘滞的: 上一条用例失败留下的错误会串到下一条,
    // 这里先读一次把它清掉。
    (void)devGetLastError();
  }
};

}  // namespace

TEST_F(DiSmokeTest, MallocMemsetMemcpyRoundtrip) {
  constexpr size_t kCount = 1024;

  void* ptr = nullptr;
  ASSERT_EQ(devMalloc(&ptr, kCount * sizeof(int)), devSuccess);
  ASSERT_NE(ptr, nullptr);

  ASSERT_EQ(devMemset(ptr, kBytePattern, kCount * sizeof(int)), devSuccess);

  std::vector<int> host(kCount, 0);
  ASSERT_EQ(
      devMemcpy(host.data(), ptr, kCount * sizeof(int), devMemcpyDeviceToHost),
      devSuccess);
  for (size_t i = 0; i < kCount; ++i) {
    ASSERT_EQ(host[i], kFilledValue) << "第 " << i << " 个元素不对";
  }

  ASSERT_EQ(devFree(ptr), devSuccess);
}

TEST_F(DiSmokeTest, AddKernelProducesExpectedResult) {
  constexpr int kCount = 1024;
  constexpr int kBlockSize = 256;

  std::vector<int> lhs(kCount);
  std::vector<int> rhs(kCount);
  std::vector<int> expected(kCount);
  for (int i = 0; i < kCount; ++i) {
    lhs[i] = i;
    rhs[i] = 2 * i;
    expected[i] = 3 * i;
  }

  int* d_lhs = nullptr;
  int* d_rhs = nullptr;
  int* d_out = nullptr;
  ASSERT_EQ(devMalloc(&d_lhs, kCount * sizeof(int)), devSuccess);
  ASSERT_EQ(devMalloc(&d_rhs, kCount * sizeof(int)), devSuccess);
  ASSERT_EQ(devMalloc(&d_out, kCount * sizeof(int)), devSuccess);

  ASSERT_EQ(
      devMemcpy(d_lhs, lhs.data(), kCount * sizeof(int), devMemcpyHostToDevice),
      devSuccess);
  ASSERT_EQ(
      devMemcpy(d_rhs, rhs.data(), kCount * sizeof(int), devMemcpyHostToDevice),
      devSuccess);

  const int blocks = (kCount + kBlockSize - 1) / kBlockSize;
  devLaunchKernelGGL(AddKernel, blocks, kBlockSize, 0, nullptr, d_lhs, d_rhs,
                     d_out, kCount);
  ASSERT_EQ(devGetLastError(), devSuccess);
  ASSERT_EQ(devDeviceSynchronize(), devSuccess);

  std::vector<int> actual(kCount, 0);
  ASSERT_EQ(devMemcpy(actual.data(), d_out, kCount * sizeof(int),
                      devMemcpyDeviceToHost),
            devSuccess);
  for (int i = 0; i < kCount; ++i) {
    ASSERT_EQ(actual[i], expected[i]) << "第 " << i << " 个元素不对";
  }

  ASSERT_EQ(devFree(d_lhs), devSuccess);
  ASSERT_EQ(devFree(d_rhs), devSuccess);
  ASSERT_EQ(devFree(d_out), devSuccess);
}

TEST_F(DiSmokeTest, SymbolCopyRoundtrip) {
  constexpr int kHostValue = 1234;

  // 符号参数必须传变量名本身: host 代码里 __device__ 变量的地址不是设备地址,
  // 传 &var 会返回 cudaErrorInvalidSymbol。
  // HIP 侧 hipMemcpyToSymbol 是 T* 模板重载, 必须传地址 —— 两边正好相反,
  // di 层需要把这组 API 改成包装函数才能同时成立。
#if defined(__PLATFORM_NVCC__)
  void* addr = nullptr;
  ASSERT_EQ(devGetSymbolAddress(&addr, gDeviceValue), devSuccess);
  EXPECT_NE(addr, nullptr);

  ASSERT_EQ(devMemcpyToSymbol(gDeviceValue, &kHostValue, sizeof(int), 0,
                              devMemcpyHostToDevice),
            devSuccess);

  int read_back = 0;
  ASSERT_EQ(devMemcpyFromSymbol(&read_back, gDeviceValue, sizeof(int), 0,
                                devMemcpyDeviceToHost),
            devSuccess);
  EXPECT_EQ(read_back, kHostValue);
#elif defined(__PLATFORM_HIPCC__)
  GTEST_SKIP() << "符号拷贝的跨后端语义差异, 待 di 层包装函数落地后再测";
#endif
}

TEST_F(DiSmokeTest, StreamAndEventLifecycle) {
  devStream_t stream = nullptr;
  ASSERT_EQ(devStreamCreate(&stream), devSuccess);

  devEvent_t start = nullptr;
  devEvent_t stop = nullptr;
  ASSERT_EQ(devEventCreate(&start), devSuccess);
  ASSERT_EQ(devEventCreate(&stop), devSuccess);

  void* scratch = nullptr;
  ASSERT_EQ(devMalloc(&scratch, 1 << 20), devSuccess);

  ASSERT_EQ(devEventRecord(start, stream), devSuccess);
  ASSERT_EQ(devMemsetAsync(scratch, 0, 1 << 20, stream), devSuccess);
  ASSERT_EQ(devEventRecord(stop, stream), devSuccess);
  ASSERT_EQ(devEventSynchronize(stop), devSuccess);

  float elapsed_ms = 0;
  ASSERT_EQ(devEventElapsedTime(&elapsed_ms, start, stop), devSuccess);
  EXPECT_GE(elapsed_ms, 0.0f);

  ASSERT_EQ(devFree(scratch), devSuccess);
  ASSERT_EQ(devEventDestroy(start), devSuccess);
  ASSERT_EQ(devEventDestroy(stop), devSuccess);
  ASSERT_EQ(devStreamDestroy(stream), devSuccess);
}

TEST_F(DiSmokeTest, OutOfMemoryIsReportedAsMemoryAllocationError) {
  void* ptr = nullptr;
  // 1TB, 任何加速器都放不下
  const devError_t err = devMalloc(&ptr, size_t(1) << 40);
  EXPECT_EQ(err, devErrorMemoryAllocation);
}

TEST_F(DiSmokeTest, DevicePropertiesAreReadable) {
  devDeviceProp_t prop{};
  ASSERT_EQ(devGetDeviceProperties(&prop, 0), devSuccess);
  EXPECT_NE(prop.name[0], '\0');

  int max_shmem = 0;
  ASSERT_EQ(
      devDeviceGetAttribute(&max_shmem, devDevAttrMaxSharedMemoryPerBlock, 0),
      devSuccess);
  EXPECT_GT(max_shmem, 0);
}

TEST_F(DiSmokeTest, CooperativeGroupTileCoversAllThreads) {
  int* d_out = nullptr;
  ASSERT_EQ(devMalloc(&d_out, kTileSize * sizeof(int)), devSuccess);
  ASSERT_EQ(devMemset(d_out, 0, kTileSize * sizeof(int)), devSuccess);

  devLaunchKernelGGL(TileRankKernel, 1, kTileSize, 0, nullptr, d_out);
  ASSERT_EQ(devGetLastError(), devSuccess);
  ASSERT_EQ(devDeviceSynchronize(), devSuccess);

  std::vector<int> host(kTileSize, 0);
  ASSERT_EQ(devMemcpy(host.data(), d_out, kTileSize * sizeof(int),
                      devMemcpyDeviceToHost),
            devSuccess);
  for (int i = 0; i < kTileSize; ++i) {
    ASSERT_EQ(host[i], 1) << "tile 内的第 " << i << " 个线程没被覆盖";
  }

  ASSERT_EQ(devFree(d_out), devSuccess);
}

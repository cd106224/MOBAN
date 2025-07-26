#pragma once

#include <hip/hip_runtime.h>

// clang-format off

// Device Management
#define devDeviceGetAttribute       hipDeviceGetAttribute
#define devDeviceReset              hipDeviceReset
#define devDeviceSynchronize        hipDeviceSynchronize
#define devGetDevice                hipGetDevice
#define devGetDeviceCount           hipGetDeviceCount
#define devGetDeviceProperties      hipGetDeviceProperties
#define devSetDevice                hipSetDevice

// Error Handling
#define devGetErrorName             hipGetErrorName
#define devGetErrorString           hipGetErrorString
#define devGetLastError             hipGetLastError
#define devPeekAtLastError          hipPeekAtLastError

// Stream Management
#define devStreamCreate             hipStreamCreate
#define devStreamDestroy            hipStreamDestroy
#define devStreamSynchronize        hipStreamSynchronize
#define devStreamWaitEvent          hipStreamWaitEvent

// Event Management
#define devEventCreate              hipEventCreate
#define devEventCreateWithFlags     hipEventCreateWithFlags
#define devEventDestroy             hipEventDestroy
#define devEventElapsedTime         hipEventElapsedTime
#define devEventRecord              hipEventRecord
#define devEventSynchronize         hipEventSynchronize

// Memory Management
#define devFree                     hipFree
#define devFreeHost                 hipHostFree
#define devGetSymbolAddress         hipGetSymbolAddress
#define devHostAlloc                hipHostAlloc
#define devMalloc                   hipMalloc
#if HIP_VERSION >= 502  // ROCm 5.2.0 introduced hipMallocAsync
#define devMallocAsync              hipMallocAsync
#endif
#define devMallocHost               hipHostMalloc
#define devMallocManaged            hipMallocManaged
#define devMemGetInfo               hipMemGetInfo
#define devMemcpy                   hipMemcpy
#define devMemcpyAsync              hipMemcpyAsync
#define devMemcpyFromSymbol(dst, symbol, ...)       hipMemcpyFromSymbol(dst, HIP_SYMBOL(symbol), __VA_ARGS__)
#define devMemcpyFromSymbolAsync(dst, symbol, ...)  hipMemcpyFromSymbolAsync(dst, HIP_SYMBOL(symbol), __VA_ARGS__)
#define devMemcpyPeer               hipMemcpyPeer
#define devMemcpyPeerAsync          hipMemcpyPeerAsync
#define devMemcpyToSymbol(symbol, ...)              hipMemcpyToSymbol(HIP_SYMBOL(symbol), __VA_ARGS__)
#define devMemcpyToSymbolAsync(symbol, ...)         hipMemcpyToSymbolAsync(HIP_SYMBOL(symbol), __VA_ARGS__)
#define devMemset                   hipMemset
#define devMemsetAsync              hipMemsetAsync

// Unified Addressing
#define devPointerGetAttributes     hipPointerGetAttributes

// Peer Device Memory Access
#define devDeviceCanAccessPeer      hipDeviceCanAccessPeer
#define devDeviceDisablePeerAccess  hipDeviceDisablePeerAccess
#define devDeviceEnablePeerAccess   hipDeviceEnablePeerAccess

// Launch Kernel
#define devLaunchKernelGGL(kernelName, numBlocks, numThreads, memPerBlock, streamId, ...) \
    hipLaunchKernelGGL(HIP_KERNEL_NAME(kernelName), dim3(numBlocks), dim3(numThreads), memPerBlock, streamId, __VA_ARGS__)

// clang-format on

#pragma once

#include <cuda.h>
#include <cuda_runtime.h>

// clang-format off

#define devDeviceGetAttribute       cudaDeviceGetAttribute
#define devDeviceReset              cudaDeviceReset
#define devDeviceSynchronize        cudaDeviceSynchronize
#define devGetDevice                cudaGetDevice
#define devGetDeviceCount           cudaGetDeviceCount
#define devGetDeviceProperties      cudaGetDeviceProperties
#define devSetDevice                cudaSetDevice

// Error Handling
#define devGetErrorName             cudaGetErrorName
#define devGetErrorString           cudaGetErrorString
#define devGetLastError             cudaGetLastError
#define devPeekAtLastError          cudaPeekAtLastError

// Stream Management
#define devStreamCreate             cudaStreamCreate
#define devStreamDestroy            cudaStreamDestroy
#define devStreamSynchronize        cudaStreamSynchronize
#define devStreamWaitEvent          cudaStreamWaitEvent

// Event Management
#define devEventCreate              cudaEventCreate
#define devEventCreateWithFlags     cudaEventCreateWithFlags
#define devEventDestroy             cudaEventDestroy
#define devEventElapsedTime         cudaEventElapsedTime
#define devEventRecord              cudaEventRecord
#define devEventSynchronize         cudaEventSynchronize

// Memory Management
#define devFree                     cudaFree
#define devFreeHost                 cudaFreeHost
#define devGetSymbolAddress         cudaGetSymbolAddress
#define devHostAlloc                cudaHostAlloc
#define devMalloc                   cudaMalloc
#if CUDART_VERSION >= 11020  // 11.2 introduced cudaMallocAsync
#define devMallocAsync              cudaMallocAsync
#endif
#define devMallocHost               cudaMallocHost
#define devMallocManaged            cudaMallocManaged
#define devMemGetInfo               cudaMemGetInfo
#define devMemcpy                   cudaMemcpy
#define devMemcpyAsync              cudaMemcpyAsync
#define devMemcpyFromSymbol         cudaMemcpyFromSymbol
#define devMemcpyFromSymbolAsync    cudaMemcpyFromSymbolAsync
#define devMemcpyPeer               cudaMemcpyPeer
#define devMemcpyPeerAsync          cudaMemcpyPeerAsync
#define devMemcpyToSymbol           cudaMemcpyToSymbol
#define devMemcpyToSymbolAsync      cudaMemcpyToSymbolAsync
#define devMemset                   cudaMemset
#define devMemsetAsync              cudaMemsetAsync

// Unified Addressing
#define devPointerGetAttributes     cudaPointerGetAttributes

// Peer Device Memory Access
#define devDeviceCanAccessPeer      cudaDeviceCanAccessPeer
#define devDeviceDisablePeerAccess  cudaDeviceDisablePeerAccess
#define devDeviceEnablePeerAccess   cudaDeviceEnablePeerAccess

// Launch Kernel
#define devLaunchKernelGGL(kernelName, numBlocks, numThreads, memPerBlock, streamId, ...)   \
    do {                                                                                    \
        kernelName<<<(numBlocks), (numThreads), (memPerBlock), (streamId)>>>(__VA_ARGS__);  \
    } while (0)

// clang-format on

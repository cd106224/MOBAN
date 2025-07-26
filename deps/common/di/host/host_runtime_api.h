#pragma once

#include "di/host/host_common.h"
#include "di/host/host_defines.h"

// clang-format off

// Device Management
#define devDeviceGetAttribute(...)      __RuntimeError(devDeviceGetAttribute)
#define devDeviceReset(...)             __RuntimeError(devDeviceReset)
#define devDeviceSynchronize(...)       __RuntimeError(devDeviceSynchronize)
#define devGetDevice(...)               __RuntimeError(devGetDevice)
#define devGetDeviceCount(...)          __RuntimeError(devGetDeviceCount)
#define devPointerGetAttributes         __RuntimeError(devPointerGetAttributes)
#define devSetDevice(...)               __RuntimeError(devSetDevice)

// Error Handling
#define devGetErrorName(...)            __RuntimeError(devGetErrorName)
#define devGetErrorString(...)          __RuntimeError(devGetErrorString)
#define devGetLastError(...)            __RuntimeError(devGetLastError)

// Stream Management
#define devStreamCreate(...)            __RuntimeError(devStreamCreate)
#define devStreamDestroy(...)           __RuntimeError(devStreamDestroy)
#define devStreamSynchronize(...)       __RuntimeError(devStreamSynchronize)
#define devStreamWaitEvent(...)         __RuntimeError(devStreamWaitEvent)

// Event Management
#define devEventCreate(...)             __RuntimeError(devEventCreate)
#define devEventCreateWithFlags(...)    __RuntimeError(devEventCreateWithFlags)
#define devEventDestroy(...)            __RuntimeError(devEventDestroy)
#define devEventElapsedTime(...)        __RuntimeError(devEventElapsedTime)
#define devEventRecord(...)             __RuntimeError(devEventRecord)
#define devEventSynchronize(...)        __RuntimeError(devEventSynchronize)

// Memory Management
#define devFree(...)                    __RuntimeError(devFree)
#define devFreeHost(...)                __RuntimeError(devFreeHost)
#define devHostAlloc(...)               __RuntimeError(devHostAlloc)
#define devMalloc(...)                  __RuntimeError(devMalloc)
#define devMallocHost(...)              __RuntimeError(devMallocHost)
#define devMallocManaged(...)           __RuntimeError(devMallocManaged)
#define devMemGetInfo(...)              __RuntimeError(devMemGetInfo)
#define devMemcpy(...)                  __RuntimeError(devMemcpy)
#define devMemcpyAsync(...)             __RuntimeError(devMemcpyAsync)
#define devMemcpyFromSymbol(...)        __RuntimeError(devMemcpyFromSymbol)
#define devMemcpyFromSymbolAsync(...)   __RuntimeError(devMemcpyFromSymbolAsync)
#define devMemcpyPeer(...)              __RuntimeError(devMemcpyPeer)
#define devMemcpyPeerAsync(...)         __RuntimeError(devMemcpyPeerAsync)
#define devMemcpyToSymbol(...)          __RuntimeError(devMemcpyToSymbol)
#define devMemcpyToSymbolAsync(...)     __RuntimeError(devMemcpyToSymbolAsync)
#define devMemset(...)                  __RuntimeError(devMemset)
#define devMemsetAsync(...)             __RuntimeError(devMemsetAsync)

// Peer Device Memory Access
#define devDeviceCanAccessPeer(...)     __RuntimeError(devDeviceCanAccessPeer)
#define devDeviceDisablePeerAccess(...) __RuntimeError(devDeviceDisablePeerAccess)
#define devDeviceEnablePeerAccess(...)  __RuntimeError(devDeviceEnablePeerAccess)

// Launch Kernel
#define devLaunchKernelGGL(kernelName, ...) __RuntimeError(kernelName)

// clang-format on

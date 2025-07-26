#pragma once

#include <driver_types.h>

// clang-format off

#define devHostAllocPortable        cudaHostAllocPortable

// types
typedef cudaError_t                 devError_t;
typedef cudaStream_t                devStream_t;
typedef cudaEvent_t                 devEvent_t;
typedef cudaPointerAttributes       devPointerAttribute_t;
typedef cudaDeviceProp              devDeviceProp_t;

// error types
#define devSuccess                  cudaSuccess
#define devErrorMemoryAllocation    cudaErrorMemoryAllocation

// stream handles
#define devStreamDefault            cudaStreamDefault
#define devStreamNonBlocking        cudaStreamNonBlocking
#define devStreamLegacy             cudaStreamLegacy
#define devStreamPerThread          cudaStreamPerThread

// event flags
#define devEventDefault             cudaEventDefault
#define devEventBlockingSync        cudaEventBlockingSync
#define devEventDisableTiming       cudaEventDisableTiming
#define devEventInterprocess        cudaEventInterprocess

// memory copy types
#define devMemcpyHostToHost         cudaMemcpyHostToHost
#define devMemcpyHostToDevice       cudaMemcpyHostToDevice
#define devMemcpyDeviceToHost       cudaMemcpyDeviceToHost
#define devMemcpyDeviceToDevice     cudaMemcpyDeviceToDevice
#define devMemcpyDefault            cudaMemcpyDefault

// memory types
#define devMemoryTypeHost           cudaMemoryTypeHost
#define devMemoryTypeDevice         cudaMemoryTypeDevice
#define devMemoryTypeManaged        cudaMemoryTypeManaged

// device attributes
#define devDevAttrMaxSharedMemoryPerBlock   cudaDevAttrMaxSharedMemoryPerBlock

// clang-format on
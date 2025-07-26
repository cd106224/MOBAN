#pragma once

#include <hip/driver_types.h>

// clang-format off

#define devHostAllocPortable        hipHostMallocPortable

// types
typedef hipError_t                  devError_t;
typedef hipStream_t                 devStream_t;
typedef hipEvent_t                  devEvent_t;
typedef hipPointerAttribute_t       devPointerAttribute_t;
typedef hipDeviceProp_t             devDeviceProp_t;

// error types
#define devSuccess                  hipSuccess
#define devErrorMemoryAllocation    hipErrorOutOfMemory

// stream handles
#define devStreamDefault            hipStreamDefault
#define devStreamNonBlocking        hipStreamNonBlocking
#define devStreamLegacy             ((hipStream_t)hipStreamDefault)  // hipStreamLegacy
#define devStreamPerThread          hipStreamPerThread

// event flags
#define devEventDefault             hipEventDefault
#define devEventBlockingSync        hipEventBlockingSync
#define devEventDisableTiming       hipEventDisableTiming
#define devEventInterprocess        hipEventInterprocess

// memory copy types
#define devMemcpyHostToHost         hipMemcpyHostToHost
#define devMemcpyHostToDevice       hipMemcpyHostToDevice
#define devMemcpyDeviceToHost       hipMemcpyDeviceToHost
#define devMemcpyDeviceToDevice     hipMemcpyDeviceToDevice
#define devMemcpyDefault            hipMemcpyDefault

// memory types
/// Please note, hipMemoryType enum values are different from cudaMemoryType enum values
#define devMemoryTypeHost           hipMemoryTypeHost
#define devMemoryTypeDevice         hipMemoryTypeDevice
#define devMemoryTypeManaged        hipMemoryTypeManaged

// device attributes
#define devDevAttrMaxSharedMemoryPerBlock   hipDeviceAttributeMaxSharedMemoryPerBlock

// clang-format on
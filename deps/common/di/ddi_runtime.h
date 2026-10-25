#pragma once

#include "di/ddi_common.h"

// clang-format off
#if defined(__PLATFORM_HIPCC__) && !defined(__PLATFORM_NVCC__)
#include "di/hip/hip_runtime.h"
#elif defined(__PLATFORM_NVCC__) && !defined(__PLATFORM_HIPCC__)
#include "di/cuda/cuda_runtime.h"
#else
#error("Must define exactly one of __PLATFORM_HIPCC__ or __PLATFORM_NVCC__");
#endif
// clang-format on
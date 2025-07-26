#pragma once

#include "di/ddi_common.h"

// clang-format off
#if defined(__PLATFORM_HIPCC__) && !defined(__PLATFORM_NVCC__)
#include <hip/hip_vector_types.h>
#elif defined(__PLATFORM_NVCC__) && !defined(__PLATFORM_HIPCC__)
#include <vector_types.h>
#else
#error("Must define exactly one of __PLATFORM_HIPCC__ or __PLATFORM_NVCC__");
#endif
// clang-format on
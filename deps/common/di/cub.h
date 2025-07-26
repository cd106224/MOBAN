#pragma once

#include "di/ddi_common.h"

// 统一 cub 库命名空间

#ifdef __PLATFORM_HIPCC__
#include <hipcub/hipcub.hpp>
namespace cub = hipcub;
#else
#include <cub/cub.cuh>
#endif  // __PLATFORM_HIPCC__

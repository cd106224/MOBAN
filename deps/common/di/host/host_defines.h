#pragma once

#if defined(__clang__) && defined(__HIP__)
#include <hip/hcc_detail/host_defines.h>
#else
#include <host_defines.h>
#endif
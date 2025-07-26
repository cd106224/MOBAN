#pragma once

#if defined(__clang__) && defined(__HIP__)
#ifndef __PLATFORM_HIPCC__
#define __PLATFORM_HIPCC__
#endif
#endif

#if defined(__NVCC__) || \
    (defined(__clang__) && defined(__CUDA__) && !defined(__HIP__))
#ifndef __PLATFORM_NVCC__
#define __PLATFORM_NVCC__
#endif
#endif
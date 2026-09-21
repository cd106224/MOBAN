
# 检查 PLATFORM 是 CUDA 还是 HIP

set(PLATFORM_DEFAULT "CUDA")
if (NOT $ENV{HIP_PLATFORM} STREQUAL "")
    set(PLATFORM_DEFAULT "HIP")
endif ()

if (NOT DEFINED PLATFORM)
    set(PLATFORM ${PLATFORM_DEFAULT})
endif ()

if ((NOT PLATFORM MATCHES "CUDA") AND (NOT PLATFORM MATCHES "HIP"))
    message(FATAL_ERROR "Unsupported platform: ${PLATFORM}.")
endif ()

set(PLATFORM_COMPILER "")

# 开启语言支持, CUDA 平台自动检测并设置架构

if (PLATFORM MATCHES "HIP")
    include(${CMAKE_CURRENT_LIST_DIR}/find_hip.cmake)
    set(PLATFORM_COMPILER "HIPCC")
else ()
    include(FindCUDAToolkit REQUIRED)
    # CUDA 11.5 的 nvcc 只支持到 GCC 11, 强制 nvcc 使用老版本宿主编译器,
    # 普通 C++ 代码仍使用系统默认(CMAKE_CXX_COMPILER)的编译器
    if (NOT DEFINED CMAKE_CUDA_HOST_COMPILER)
        find_program(CUDA_HOST_GXX g++-9 g++-10 g++-11)
        if (CUDA_HOST_GXX)
            set(CMAKE_CUDA_HOST_COMPILER "${CUDA_HOST_GXX}")
        endif ()
    endif ()

    if (NOT CMAKE_CUDA_ARCHITECTURES)
        set(CMAKE_CUDA_COMPILER "${CUDAToolkit_NVCC_EXECUTABLE}")
        include(${CMAKE_CURRENT_LIST_DIR}/detect_architectures.cmake)
        set(possible_archs_var "")
        set(gpu_archs "")
        rapids_cuda_detect_architectures(possible_archs_var gpu_archs)
        set(CMAKE_CUDA_ARCHITECTURES ${gpu_archs})
    endif ()

    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -Wno-deprecated-gpu-targets")
    set(CUDA_INCLUDE_DIRS "${CUDAToolkit_INCLUDE_DIRS}")
    set(PLATFORM_COMPILER "NVCC")
endif ()

message(STATUS "Configured Platform: ${PLATFORM}")

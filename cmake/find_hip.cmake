# 寻找 HIP 开发包

find_path(HIP_INCLUDE_DIR
        NAMES "hip/hip_runtime.h"
        "hip/hip_runtime_api.h"
        PATHS "${HIP_ROOT_DIR}"
        ENV ROCM_PATH
        ENV HIP_PATH
        "/opt/rocm"
        "/opt/rocm/hip"
        PATH_SUFFIXES "include"
        DOC "HIP include directory"
)

find_library(HIP_amdhip64_LIBRARY
        NAMES "amdhip64"
        PATHS "${HIP_ROOT_DIR}"
        ENV ROCM_PATH
        ENV HIP_PATH
        "/opt/rocm"
        "/opt/rocm/hip"
        PATH_SUFFIXES "lib" "lib64"
        DOC "HIP Runtime Library"
)

# 找不到就立刻报错: 这两个变量现在会被挂到 target 上, NOTFOUND 会变成
# "include 目录不存在" 这种难懂的 CMake 错误。
if (NOT HIP_INCLUDE_DIR OR NOT HIP_amdhip64_LIBRARY)
    message(FATAL_ERROR
            "HIP development package not found. "
            "Set HIP_ROOT_DIR / ROCM_PATH / HIP_PATH, or configure with -DPLATFORM=CUDA.")
endif ()

set(HIP_INCLUDE_DIRS ${HIP_INCLUDE_DIR})
set(HIP_LIBRARIES ${HIP_amdhip64_LIBRARY})

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

set(HIP_INCLUDE_DIRS ${HIP_INCLUDE_DIR})
set(HIP_LIBRARIES ${HIP_amdhip64_LIBRARY})

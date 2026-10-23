# ccache 编译缓存
# 必须在 project() 之后引入：project() 会初始化各语言的 COMPILER_LAUNCHER 并覆盖此处的设置
# 同时要早于 enable_language()，否则 CUDA/HIP 的 launcher 会失效

option(USE_CCACHE "Use ccache to cache compilation results" ON)

if (USE_CCACHE)
    # 可通过 -DCCACHE_PROGRAM=/path/to/ccache 指定可执行文件路径
    find_program(CCACHE_PROGRAM NAMES ccache DOC "ccache compiler cache")

    if (NOT CCACHE_PROGRAM)
        message(WARNING "USE_CCACHE is ON but ccache was not found, building without compiler cache")
        set(USE_CCACHE OFF)
    else ()
        # 用 COMPILER_LAUNCHER 而非改写编译器路径，与生成器无关
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_ASM_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        # CUDA/HIP 的 launcher 必须在 enable_language() 之前设置
        # 注意：ccache 3.x 能按 .cu 后缀缓存 nvcc，但认不出 nvcc 的 -x cu 选项
        # (CMake 编译 .cu 时一定会带 -x cu)，命中会退化成 unsupported source language 直接透传；
        # ccache 4.x 才补上 -x cu 的支持。HIP 的 hipcc 一律不被支持
        set(CMAKE_CUDA_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_HIP_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")

        set(CCACHE_DIR "$ENV{HOME}/.cache/ccache" CACHE PATH "Directory holding the ccache cache files")
        # content 比 mtime 慢一点，但不会因时间戳被保留而命中过期的缓存
        set(CCACHE_COMPILERCHECK "content" CACHE STRING "How ccache detects compiler changes: mtime or content")
        set(ENV{CCACHE_DIR} "${CCACHE_DIR}")
        set(ENV{CCACHE_COMPILERCHECK} "${CCACHE_COMPILERCHECK}")

        message(STATUS "ccache enabled: ${CCACHE_PROGRAM} (dir=${CCACHE_DIR}, compiler_check=${CCACHE_COMPILERCHECK})")
    endif ()
endif ()

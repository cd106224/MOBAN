# di 层的测试
#
# di_compile_test.cu: 只编译不运行, 检查 CUDA/HIP 的宏映射签名是否对得上。
#   不需要 GPU, CI 里也能跑。
# di_smoke_test.cu: 真跑, 验证映射在运行时也成立。没有设备时测试自己 skip。

if (PLATFORM STREQUAL "HIP")
    set_source_files_properties(
            di_compile_test.cu
            di_smoke_test.cu
            PROPERTIES LANGUAGE HIP)
endif ()

add_library(di_compile_check OBJECT di_compile_test.cu)
target_include_directories(di_compile_check SYSTEM PRIVATE ${PLATFORM_INCLUDE_DIRS})
target_link_libraries(di_compile_check PRIVATE lib_common)

option(MOBAN_BUILD_GPU_TESTS "构建需要真实加速器运行的测试" ON)
if (MOBAN_BUILD_GPU_TESTS)
    _add_gtest("di_smoke_test.cu" LINK_LIBS
            lib_common
            _gtest_main
            _gtest
            pthread
            ${PLATFORM_LIBRARIES})
    # 目标名由 _add_gtest 从源文件名推导, 见 cmake/gtest_helpers.cmake
    target_include_directories(di_smoke_test SYSTEM PRIVATE ${PLATFORM_INCLUDE_DIRS})
endif ()

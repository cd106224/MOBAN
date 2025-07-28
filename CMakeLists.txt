cmake_minimum_required(VERSION 3.26)

project(MOBAN LANGUAGES C CXX ASM)
# 检测 GPU 编译环境
#include(cmake/configure_gpu_platform.cmake)
#enable_language(${PLATFORM})

#include(cmake/clang_format.cmake)

find_package(Threads REQUIRED)

include(cmake/glob_source.cmake)
include(cmake/sanitize.cmake)
include(cmake/gtest_helpers.cmake)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(deps/common)
add_subdirectory(deps/third_party)
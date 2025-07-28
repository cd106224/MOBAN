cmake_minimum_required(VERSION 3.26)

project(MOBAN LANGUAGES C CXX ASM)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Building on/for Windows")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building on/for Linux")
    # 检测 GPU 编译环境
    include(cmake/configure_gpu_platform.cmake)
    enable_language(${PLATFORM})
    include(cmake/clang_format.cmake)
endif ()

include(cmake/find_thread.cmake)
include(cmake/glob_source.cmake)
include(cmake/sanitize.cmake)
include(cmake/gtest_helpers.cmake)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(deps/common)
add_subdirectory(deps/third_party)
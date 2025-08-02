cmake_minimum_required(VERSION 3.26)

project(MOBAN LANGUAGES C CXX ASM)

# 检测 GPU 编译环境
include(cmake/configure_gpu_platform.cmake)
enable_language(${PLATFORM})

include(cmake/clang_format.cmake)
include(cmake/arch.cmake)
include(cmake/find_thread.cmake)
include(cmake/glob_source.cmake)
include(cmake/sanitize.cmake)
include(cmake/gtest_helpers.cmake)
include(cmake/verion.cmake)

set(CMAKE_CXX_STANDARD 17)
add_library(global-group INTERFACE)

message(STATUS "MOBAN: Build ${MOBAN_BACKEND_VERSION}")
add_definitions(-D_BACKEND_VERSION="${MOBAN_BACKEND_VERSION}")

add_subdirectory(deps/common)
add_subdirectory(deps/third_party)
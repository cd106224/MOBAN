cmake_minimum_required(VERSION 3.26)

project(MOBAN LANGUAGES C CXX)
# 检测 GPU 编译环境
include(cmake/configure_gpu_platform.cmake)
enable_language(${PLATFORM})

include(cmake/clang_format.cmake)
include(cmake/sanitize.cmake)

set(CMAKE_CXX_STANDARD 20)
# di 层: 设备无关的加速器接口抽象, 当前有 CUDA / HIP / host 三路后端。
#
# 头文件归入 lib_common, 测试在 tests/ 子目录下维护。

file(GLOB di_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
target_sources(lib_common PRIVATE ${di_headers})

add_subdirectory(tests)

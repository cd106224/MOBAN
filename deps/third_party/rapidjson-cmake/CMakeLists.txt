set(RAPIDJSON_SOURCE_DIR "${CMAKE_SOURCE_DIR}/deps/third_party/rapidjson")

add_library(rapidjson INTERFACE)

# 与仓库内其它第三方库一致，以 SYSTEM 方式引入，避免 rapidjson 自身的告警被 -Werror 升级为错误
target_include_directories(rapidjson SYSTEM INTERFACE ${RAPIDJSON_SOURCE_DIR}/include)

# rapidjson 1.1.0 给 const 成员赋值，GCC 15 新增的 -Wtemplate-body 会将其判为错误
# 用编译器探测而非 CMAKE_CXX_COMPILER_VERSION，避免构建目录缓存了旧的编译器版本
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-Wtemplate-body")
check_cxx_source_compiles("int main() { return 0; }" RAPIDJSON_HAS_WTEMPLATE_BODY)
unset(CMAKE_REQUIRED_FLAGS)
if(RAPIDJSON_HAS_WTEMPLATE_BODY)
    target_compile_options(rapidjson INTERFACE -Wno-template-body)
endif()

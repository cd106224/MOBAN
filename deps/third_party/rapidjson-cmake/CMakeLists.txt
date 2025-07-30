set(RAPIDJSON_SOURCE_DIR "${CMAKE_SOURCE_DIR}/deps/third_party/rapidjson")

add_library(rapidjson INTERFACE)

target_include_directories(rapidjson INTERFACE ${RAPIDJSON_SOURCE_DIR}/include)
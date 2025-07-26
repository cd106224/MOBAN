add_headers_and_sources(base src)
add_library(lib_base STATIC ${base_header} ${base_sources})
target_include_directories(lib_base PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_headers_and_sources(base_tests tests)
_add_gtest(${base_tests_sources} lib_base)


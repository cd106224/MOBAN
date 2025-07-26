add_headers_and_sources(common base)
add_headers_and_sources(common dmm/detail)
add_headers_and_sources(common dmm/mr/device)
add_headers_and_sources(common dmm/mr/device/detail)
add_headers_and_sources(common dmm/mr/host)
add_headers_and_sources(common dmm/mr)
add_headers_and_sources(common dmm)
add_library(lib_common STATIC ${common_header} ${common_sources})
target_include_directories(lib_common
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_headers_and_sources(base_tests tests)
_add_gtest(${base_tests_sources} lib_common)


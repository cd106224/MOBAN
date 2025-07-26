add_headers_and_sources(common base)
add_library(lib_common STATIC ${common_header} ${common_sources})
target_include_directories(lib_common
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_headers_and_sources(base_tests tests)
_add_gtest("${base_tests_sources}" LINK_LIBS
        lib_common
        _gtest_main
        _gtest
        pthread)


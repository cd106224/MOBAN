add_headers_and_sources(common base)
add_headers_and_sources(common di)
add_headers_and_sources(common log)
add_headers_and_sources(common utilities)
add_library(lib_common STATIC ${common_header} ${common_sources})
target_link_libraries(lib_common PUBLIC ch_contrib::spdlog)
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


add_headers_and_sources(common base)
add_headers_and_sources(common di)
add_headers_and_sources(common log)
add_headers_and_sources(common utilities)
add_headers_and_sources(common config)

add_library(lib_common STATIC ${common_header} ${common_sources})
target_link_libraries(lib_common PUBLIC
        ch_contrib::spdlog
        boost::headers_only)
target_link_libraries(lib_common PRIVATE rapidjson)

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

add_headers_and_sources(config_tests tests/config_test)
_add_gtest("${config_tests_sources}" LINK_LIBS
        lib_common
        _gtest_main
        _gtest
        pthread
        COMPILE_DEFINITIONS MOBAN_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/config_test/data")


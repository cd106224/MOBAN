add_headers_and_sources(Muduo muduo/net)
add_library(plugin_Muduo STATIC ${Muduo_header} ${Muduo_sources})
target_link_libraries(plugin_Muduo
        lib_common
        boost::headers_only
)

target_include_directories(plugin_Muduo
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_glob(Muduo_tests tests/*.cpp)
_add_gtest("${Muduo_tests}" LINK_LIBS
        plugin_Muduo
        _gtest_main
        _gtest
        pthread)
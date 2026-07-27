add_headers_and_sources(Muduo net)
add_library(plugin_Muduo STATIC ${Muduo_header} ${Muduo_sources})
target_link_libraries(plugin_Muduo
        lib_common
        boost::headers_only
)
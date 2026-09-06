add_headers_and_sources(Miniftpd src)
add_executable(example_Miniftpd ${Miniftpd_header} ${Miniftpd_sources})
target_link_libraries(example_Miniftpd
    lib_common
    plugin_Muduo
)
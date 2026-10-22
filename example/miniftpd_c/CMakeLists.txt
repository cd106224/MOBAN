add_headers_and_sources(Miniftpd_c src)
add_executable(example_Miniftpd_c ${Miniftpd_c_header} ${Miniftpd_c_sources})
target_link_libraries(example_Miniftpd_c
    lib_common
    plugin_Muduo
)
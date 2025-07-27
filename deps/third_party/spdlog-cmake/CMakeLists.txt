
set(SPDLOG_DIR "${CMAKE_SOURCE_DIR}/deps/third_party/spdlog")

set(SRCS
        ${SPDLOG_DIR}/src/async.cpp
        ${SPDLOG_DIR}/src/cfg.cpp
        ${SPDLOG_DIR}/src/color_sinks.cpp
        ${SPDLOG_DIR}/src/file_sinks.cpp
        ${SPDLOG_DIR}/src/spdlog.cpp
        ${SPDLOG_DIR}/src/stdout_sinks.cpp
)

add_library(_spdlog ${SRCS})
add_library(ch_contrib::spdlog ALIAS _spdlog)

target_compile_definitions(_spdlog PUBLIC SPDLOG_COMPILED_LIB)
target_include_directories(_spdlog PUBLIC ${SPDLOG_DIR}/include)

target_link_libraries(_spdlog PUBLIC Threads::Threads)
set_target_properties(_spdlog PROPERTIES DEBUG_POSTFIX d)

target_compile_definitions(_spdlog PUBLIC SPDLOG_FMT_EXTERNAL)
target_link_libraries(_spdlog PRIVATE ch_contrib::fmt)


set(FMT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/deps/third_party/fmt")

set(SRCS
        # NOTE: do not build module for now:
        # ${FMT_SOURCE_DIR}/src/fmt.cc
        ${FMT_SOURCE_DIR}/src/format.cc
        ${FMT_SOURCE_DIR}/src/os.cc

        ${FMT_SOURCE_DIR}/include/fmt/args.h
        ${FMT_SOURCE_DIR}/include/fmt/chrono.h
        ${FMT_SOURCE_DIR}/include/fmt/color.h
        ${FMT_SOURCE_DIR}/include/fmt/compile.h
        ${FMT_SOURCE_DIR}/include/fmt/core.h
        ${FMT_SOURCE_DIR}/include/fmt/format.h
        ${FMT_SOURCE_DIR}/include/fmt/format-inl.h
        ${FMT_SOURCE_DIR}/include/fmt/os.h
        ${FMT_SOURCE_DIR}/include/fmt/ostream.h
        ${FMT_SOURCE_DIR}/include/fmt/printf.h
        ${FMT_SOURCE_DIR}/include/fmt/ranges.h
        ${FMT_SOURCE_DIR}/include/fmt/std.h
        ${FMT_SOURCE_DIR}/include/fmt/xchar.h
)

add_library(_fmt ${SRCS})
target_include_directories(_fmt SYSTEM PUBLIC ${FMT_SOURCE_DIR}/include)
add_library(ch_contrib::fmt ALIAS _fmt)

#clang-format

set(BUILD_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/build_support")
set(CLANG_SEARCH_PATH "/usr/local/bin" "/usr/bin" "/usr/local/opt/llvm/bin")

find_program(CLANG_FORMAT_BIN
        NAMES clang-format
        HINTS ${CLANG_SEARCH_PATH})

if ("${CLANG_FORMAT_BIN}" STREQUAL "CLANG_FORMAT_BIN-NOTFOUND")
    message(WARNING "Couldn't find clang-format.")
    return()
else ()
    message(STATUS "Found clang-format at ${CLANG_FORMAT_BIN}")
endif ()

string(CONCAT FORMAT_DIRS
        "${CMAKE_SOURCE_DIR}/deps/base,"
        "${CMAKE_SOURCE_DIR}/tests,"
)

add_custom_target(format ${BUILD_SUPPORT_DIR}/run_clang_format.py
        ${CLANG_FORMAT_BIN}
        ${BUILD_SUPPORT_DIR}/clang_format_exclusions.txt
        --source_dirs
        ${FORMAT_DIRS}
        --fix
        --quiet
)

add_custom_target(check-format ${BUILD_SUPPORT_DIR}/run_clang_format.py
        ${CLANG_FORMAT_BIN}
        ${BUILD_SUPPORT_DIR}/clang_format_exclusions.txt
        --source_dirs
        ${FORMAT_DIRS}
        --quiet
)
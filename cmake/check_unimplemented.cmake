#check-unimplemented

set(BUILD_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/build_support")

find_program(PYTHON3_BIN NAMES python3 python)

if ("${PYTHON3_BIN}" STREQUAL "PYTHON3_BIN-NOTFOUND")
    message(WARNING "Couldn't find python3.")
    return()
else ()
    message(STATUS "Found python3 at ${PYTHON3_BIN}")
endif ()

# 只检查项目自己的代码，不扫 deps/third_party 等第三方库
set(CHECK_DIRS
        "${CMAKE_SOURCE_DIR}/deps/common"
        "${CMAKE_SOURCE_DIR}/tests"
        "${CMAKE_SOURCE_DIR}/plugins"
)

add_custom_target(check-unimplemented
        ${PYTHON3_BIN} ${BUILD_SUPPORT_DIR}/check_unimplemented.py
        ${CHECK_DIRS}
        COMMENT "Checking for member functions declared but not implemented..."
)

option(SANITIZE "Enable one of the code sanitizers" "")

set(SAN_FLAGS "${SAN_FLAGS} -g -fno-omit-frame-pointer")

if (SANITIZE)
    if (SANITIZE STREQUAL "address")
        set(ASAN_FLAGS "-fsanitize=address -fsanitize-address-use-after-scope")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SAN_FLAGS} ${ASAN_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SAN_FLAGS} ${ASAN_FLAGS}")
    else ()
        message(FATAL_ERROR "Unknown sanitizer type: ${SANITIZE}")
    endif ()
endif ()
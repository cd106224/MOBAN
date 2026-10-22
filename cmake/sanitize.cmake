option(SANITIZE "Enable one of the code sanitizers: address / undefined / address,undefined" "")

if (SANITIZE)
    # 按逗号切分，允许同时开启多个 sanitizer
    string(REPLACE "," ";" SANITIZE_LIST "${SANITIZE}")
    set(SANITIZER_FLAGS)
    foreach(sanitizer ${SANITIZE_LIST})
        string(STRIP "${sanitizer}" sanitizer)
        if (sanitizer STREQUAL "address")
            list(APPEND SANITIZER_FLAGS -fsanitize=address -fsanitize-address-use-after-scope)
        elseif (sanitizer STREQUAL "undefined")
            # 默认 UBSan 报错后继续执行并返回 0，CI 里容易被忽略，这里改成致命错误
            list(APPEND SANITIZER_FLAGS -fsanitize=undefined -fno-sanitize-recover=all)
        else ()
            message(FATAL_ERROR "Unknown sanitizer type: ${sanitizer}")
        endif ()
    endforeach ()

    set(SAN_FLAGS -g -fno-omit-frame-pointer ${SANITIZER_FLAGS})
    string(JOIN " " SAN_FLAGS_STR ${SAN_FLAGS})
    message(STATUS "Sanitizers enabled: ${SANITIZE}")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SAN_FLAGS_STR}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SAN_FLAGS_STR}")
endif ()

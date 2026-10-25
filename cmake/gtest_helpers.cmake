enable_testing()
include(GoogleTest)

add_custom_target(build-tests COMMAND ${CMAKE_CTEST_COMMAND} --show-only)
add_custom_target(check-tests COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

function(_add_gtest TEST_SOURCES)
    set(DEFAULT_LIBS _gtest_main _gtest pthread)
    set(options "")
    set(one_value_args "")
    # 未声明的 keyword 不会被报错，而是被吞进紧邻的 multi_value_args，
    # 所以每加一个可选参数都必须在这里登记。
    set(multi_value_args LINK_LIBS COMPILE_DEFINITIONS)
    cmake_parse_arguments(_add_gtest "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
    if (NOT _add_gtest_LINK_LIBS)
        set(_add_gtest_LINK_LIBS ${DEFAULT_LIBS})
    endif ()
    foreach (test_source ${TEST_SOURCES})
        # 源文件可能是 .cpp / .cc / .cu, 用 NAME_WE 去掉扩展名。
        # 原来只剥 ".cpp", .cu 会生成带点的非法 target 名。
        get_filename_component(test_name ${test_source} NAME_WE)
        add_executable(${test_name} EXCLUDE_FROM_ALL ${test_source})
        add_dependencies(build-tests ${test_name})
        add_dependencies(check-tests ${test_name})
        gtest_discover_tests(${test_name}
                EXTRA_ARGS
                --gtest_color=auto
                --gtest_output=xml:${CMAKE_BINARY_DIR}/test/${test_name}.xml
                --gtest_catch_exceptions=0
                DISCOVERY_TIMEOUT 120
                PROPERTIES
                TIMEOUT 120
        )
        target_link_libraries(${test_name} PRIVATE
                ${_add_gtest_LINK_LIBS}
        )
        if (_add_gtest_COMPILE_DEFINITIONS)
            target_compile_definitions(${test_name} PRIVATE
                    ${_add_gtest_COMPILE_DEFINITIONS}
            )
        endif ()
        set_target_properties(${test_name}
                PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
        )
    endforeach ()
endfunction()
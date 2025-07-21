enable_testing()
include(GoogleTest)

add_custom_target(build-tests COMMAND ${CMAKE_CTEST_COMMAND} --show-only)
add_custom_target(check-tests COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

function(_add_gtest TEST_SOURCES TEST_LIBRARIES)
    foreach (test_source ${TEST_SOURCES})
        get_filename_component(test_filename ${test_source} NAME)
        string(REPLACE ".cpp" "" test_name ${test_filename})
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
                ${TEST_LIBRARIES}
                _gtest_main
                _gtest
                pthread
        )
        set_target_properties(${test_name}
                PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
        )
    endforeach ()
endfunction()
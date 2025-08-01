
set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL 1)

# add_thirdparty cmake_folder[ base_folder1[, ...base_folderN]]
function(add_thirdparty cmake_folder)
    if (ARGN)
        set(base_folders ${ARGN})
    else()
        set(base_folders ${cmake_folder})
    endif()

    foreach (base_folder ${base_folders})
        # some typos in the code
        if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${base_folder}")
            message(FATAL_ERROR "No such base folder '${base_folder}' (for '${cmake_folder}' cmake folder). Typo in the base folder name?")
        endif()

        file(GLOB thirdparty_files "${base_folder}/*")
        if (NOT thirdparty_files)
            message(FATAL_ERROR "Submodule ${base_folder} is missing or empty. Typo in the submodule name?")
        endif()
    endforeach()

    message(STATUS "Adding thirdparty module ${base_folders} (configuring with ${cmake_folder})")
    add_subdirectory(${cmake_folder})
endfunction()

add_thirdparty(abseil-cpp-cmake abseil-cpp)
add_thirdparty(googletest-cmake googletest)
add_thirdparty(google-protobuf-cmake google-protobuf)
add_thirdparty(zlib-ng-cmake zlib-ng)
add_thirdparty(zstd-cmake zstd)
add_thirdparty(boost-cmake boost)
add_thirdparty(fmt-cmake fmt)
add_thirdparty(spdlog-cmake spdlog)
add_thirdparty(rapidjson-cmake rapidjson)
add_thirdparty(curl-cmake curl)

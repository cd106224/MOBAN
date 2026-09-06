option(ENABLE_EXAMPLE "Enable example" ON)

if(NOT ENABLE_PLUGINS OR NOT ENABLE_EXAMPLE)
    message(STATUS "example disabled: Skipping example build")
    return()
else()
    message(STATUS "example enabled: Building example")
endif()

add_subdirectory(miniftpd)
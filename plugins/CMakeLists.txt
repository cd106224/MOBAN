option(ENABLE_PLUGINS "Enable plugins" ON)

if (NOT ENABLE_PLUGINS)
    message(STATUS "Plugins disabled: Skipping plugin build")
    return()
else ()
    message(STATUS "Plugins enabled: Building plugins")
endif ()

add_subdirectory(Muduo)
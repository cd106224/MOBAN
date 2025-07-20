include_guard(GLOBAL)

function(configure_ptds project platform)
    target_compile_definitions(${project} PUBLIC DT_USE_PTDS)

    if (${platform} STREQUAL "CUDA")
        target_compile_definitions(${project} PUBLIC
                CUDA_API_PER_THREAD_DEFAULT_STREAM
        )
    else ()
        target_compile_options(${project} PUBLIC -fgpu-default-stream=per-thread)
    endif ()
endfunction()
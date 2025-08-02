set(MOBAN_BACKEND_VERSION "")

execute_process(COMMAND which git OUTPUT_VARIABLE GIT)

if (GIT)
    execute_process(COMMAND date +%m/%d/%Y
            OUTPUT_STRIP_TRAILING_WHITESPACE
            OUTPUT_VARIABLE DATE)
    execute_process(COMMAND git rev-parse --short=8 HEAD
            OUTPUT_STRIP_TRAILING_WHITESPACE
            OUTPUT_VARIABLE GITHASH)
    set(MOBAN_BACKEND_VERSION "${GITHASH} (Built on ${DATE})")
endif ()

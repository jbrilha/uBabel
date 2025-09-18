if(DEFINED BUILD_PICO)
    include(micro_babel_common_events.cmake)
else()
    idf_component_register(SRCS "common_events.c"
                           INCLUDE_DIRS "."
                          REQUIRES micro_babel_event micro_babel_comm
    )
endif()

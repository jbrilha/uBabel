if(DEFINED BUILD_PICO)
    include(micro_babel_event.cmake)
else()
    idf_component_register(SRCS "event.c" "event_dispatcher.c"
                          INCLUDE_DIRS "." "../../shared"
                          REQUIRES lwip micro_babel_proto_manager
    )
endif()

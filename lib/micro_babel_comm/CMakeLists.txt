if(DEFINED BUILD_PICO)
    include(micro_babel_comm.cmake)
else()
    idf_component_register(SRCS "comm_manager.c" "message_parse.c"
                          INCLUDE_DIRS "." "../../shared"
                          REQUIRES lwip micro_babel_event tcp_udp micro_babel_network micro_babel_common_events
    )
endif()

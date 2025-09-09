if(DEFINED BUILD_PICO)
    include(proto_simple_overlay.cmake)
else()
idf_component_register(SRCS "proto_simple_overlay.c"
                        INCLUDE_DIRS "." "../../shared/"
                        REQUIRES micro_babel_event micro_babel_comm
)
endif()

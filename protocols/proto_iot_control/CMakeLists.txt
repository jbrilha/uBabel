if(DEFINED BUILD_PICO)
    include(proto_iot_control.cmake)
else()
idf_component_register(SRCS "proto_iot_control.c"
                        INCLUDE_DIRS "." "../../shared/"
                        REQUIRES micro_babel_event micro_babel_comm
)
endif()

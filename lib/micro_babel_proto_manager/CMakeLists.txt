if(DEFINED BUILD_PICO)
    include(micro_babel_proto_manager.cmake)
else()
    idf_component_register(SRCS "proto_manager.c"
                          INCLUDE_DIRS "." "../../shared"
                          REQUIRES "lwip")
endif()

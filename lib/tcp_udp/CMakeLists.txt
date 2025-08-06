if(DEFINED BUILD_PICO)
    include(tcp.cmake)
    include(udp.cmake)
else()
    idf_component_register(SRCS "tcp.c" "udp.c"
                          INCLUDE_DIRS "." "../../shared"
                          REQUIRES "lwip")
endif()

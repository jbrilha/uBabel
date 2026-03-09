if(DEFINED BUILD_PICO)
    include(iperf_hal.cmake)
else()
idf_component_register(SRCS iperf_hal_esp32.c
                        INCLUDE_DIRS "." "../../shared"
                        PRIV_REQUIRES micro_babel_network
)
endif()

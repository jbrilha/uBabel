if(DEFINED BUILD_PICO)
    include(dht11.cmake)
else()
idf_component_register(SRCS "dht11.c"
                        INCLUDE_DIRS "." "../../shared/"
                        REQUIRES gpio_hal
                        PRIV_REQUIRES time_hal
)
endif()

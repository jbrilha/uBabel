if(DEFINED BUILD_PICO)
    include(lora.cmake)
else()
idf_component_register(SRCS "lora.c" "sx126x.c" "sx127x.c" 
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES spi_hal gpio_hal time_hal micro_babel_event
)
endif()

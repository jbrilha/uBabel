if(DEFINED BUILD_PICO)
    include(gpio_hal.cmake)
else()
idf_component_register(SRCS "gpio_hal.c"
                        INCLUDE_DIRS "."
                        REQUIRES driver
)
endif()

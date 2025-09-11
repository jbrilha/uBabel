if(DEFINED BUILD_PICO)
    include(tca9548.cmake)
else()
idf_component_register(SRCS "tca9548.c"
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES i2c_hal
)
endif()

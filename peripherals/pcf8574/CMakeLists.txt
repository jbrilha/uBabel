if(DEFINED BUILD_PICO)
    include(pcf8574.cmake)
else()
idf_component_register(SRCS "pcf8574.c"
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES i2c_hal
)
endif()

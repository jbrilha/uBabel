if(DEFINED BUILD_PICO)
    include(lcd16x2.cmake)
else()
idf_component_register(SRCS "lcd16x2.c"
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES i2c_hal
)
endif()

if(DEFINED BUILD_PICO)
    include(bmp280.cmake)
else()
idf_component_register(SRCS "bmp280.c"
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES i2c_hal
)
endif()

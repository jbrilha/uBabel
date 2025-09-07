if(DEFINED BUILD_PICO)
    include(i2c_hal.cmake)
else()
idf_component_register(SRCS "i2c_hal.c"
                        INCLUDE_DIRS "." "../../shared"
                        PRIV_REQUIRES driver
)
endif()

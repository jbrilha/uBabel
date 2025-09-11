if(DEFINED BUILD_PICO)
    include(paj7620.cmake)
else()
idf_component_register(SRCS "paj7620.c"
                        INCLUDE_DIRS "." "../../shared/"
                        PRIV_REQUIRES i2c_hal
)
endif()

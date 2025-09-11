if(DEFINED BUILD_PICO)
    include(mma7660.cmake)
else()
idf_component_register(SRCS "mma7660.c"
                        INCLUDE_DIRS "." "../../shared/"
                        PRIV_REQUIRES i2c_hal
)
endif()

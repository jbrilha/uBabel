if(DEFINED BUILD_PICO)
    include(i2c_hal.cmake)
else()
idf_component_register(SRCS i2c_hal_esp32.c
                        INCLUDE_DIRS "." "../../shared"
                        REQUIRES esp_driver_i2c
)
endif()

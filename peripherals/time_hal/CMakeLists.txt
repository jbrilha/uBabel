if(DEFINED BUILD_PICO)
    include(time_hal.cmake)
else()
idf_component_register(SRCS time_hal_esp32.c
                        INCLUDE_DIRS "."
                        REQUIRES esp_timer
)
endif()

if(DEFINED BUILD_PICO)
    include(spi_hal.cmake)
else()
    idf_component_register(SRCS spi_hal_esp32.c
                            INCLUDE_DIRS "." "../../shared"
                            REQUIRES esp_timer driver gpio_hal esp_driver_spi
    )
endif()

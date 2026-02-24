if(DEFINED BUILD_PICO)
    include(adc_hal.cmake)
else()
idf_component_register(SRCS adc_hal_esp32.c
                        INCLUDE_DIRS "." "../../shared"
                        PRIV_REQUIRES esp_adc
)
endif()

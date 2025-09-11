if(DEFINED BUILD_PICO)
    include(loudness_sensor.cmake)
else()
idf_component_register(SRCS "loudness_sensor.c"
                        INCLUDE_DIRS "." "../../shared"
                        PRIV_REQUIRES adc_hal
)
endif()

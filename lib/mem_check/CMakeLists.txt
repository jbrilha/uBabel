if(DEFINED BUILD_PICO)
    include(mem_check.cmake)
else()
    idf_component_register(SRCS "mem_check.c"
                          INCLUDE_DIRS "." "../../shared"
                          REQUIRES spi_flash esp_partition app_update
    )
endif()

if(DEFINED BUILD_PICO)
    include(micro_babel_network.cmake)
else()
    idf_component_register(SRCS "network_hal_esp32.c" "network_config.c" "network_manager.c"
                          INCLUDE_DIRS "." "../../shared"
                          REQUIRES esp_wifi esp_netif esp_event nvs_flash lwip micro_babel_event
    )
endif()

set(srcs
    "zigbee.c"
    "zcl_utility.c"
)

if(CONFIG_ZB_ZCZR)
    list(APPEND srcs "zb_coordinator.c" "switch_driver.c")
else()
    list(APPEND srcs "zb_end_device.c" "light_driver.c")
endif()

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES ieee802154 espressif__esp-zigbee-lib esp_driver_gpio
)

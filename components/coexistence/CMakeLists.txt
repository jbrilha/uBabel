idf_component_register(SRCS coexistence.c board.c bluetooth.c
                        INCLUDE_DIRS "."
                        REQUIRES bt ble_mesh driver esp_driver_gpio
)

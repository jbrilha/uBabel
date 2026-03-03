set(srcs
    "ble_central.c"
    "ble_peripheral.c"
    "ble_print_utils.c"
    "ble_peer_utils.c"
    "bt_host.c"
    "gatt_server.c"
)

idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "."
                        REQUIRES bt
)

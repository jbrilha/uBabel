idf_component_register(SRCS "esp32c6_buttons.c"
                        INCLUDE_DIRS "."
                        REQUIRES gpio_hal micro_babel_event micro_babel_common_events
                        PRIV_REQUIRES pcf8574
)

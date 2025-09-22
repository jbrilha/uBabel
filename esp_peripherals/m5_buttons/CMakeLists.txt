idf_component_register(SRCS "m5_buttons.c"
                        INCLUDE_DIRS "." "../../shared/"
                        REQUIRES gpio_hal micro_babel_event micro_babel_common_events
)

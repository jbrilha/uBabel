idf_component_register(SRCS ui_manager.c ui_event_monitor.c
                        INCLUDE_DIRS "."
                        REQUIRES lvgl_ui micro_babel_event spi_lcd_touch micro_babel_common_events
)

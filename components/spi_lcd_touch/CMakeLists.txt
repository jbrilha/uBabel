idf_component_register(SRCS spi_lcd_touch.c
                        INCLUDE_DIRS "."
                        REQUIRES esp_timer lvgl_ui micro_babel_event ui_event_manager
)

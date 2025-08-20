idf_component_register(SRCS "spi_lcd_touch.c" "lvgl_demo_ui.c" "lvgl_temp_bar_ui.c" "lvgl_flex_ui.c"
                        INCLUDE_DIRS "." "../../shared"
                        REQUIRES micro_babel_event
)

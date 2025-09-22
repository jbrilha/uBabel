idf_component_register(SRCS lvgl_tardis_widget.c
                        INCLUDE_DIRS "."
                        REQUIRES micro_babel_network micro_babel_event lvgl__lvgl proto_iot_control
)

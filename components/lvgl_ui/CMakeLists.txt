set(COMP_SRCS 
    lvgl_flex_layout.c
    lvgl_temperature_widget.c
    lvgl_messenger_widget.c
)

idf_component_register(SRCS ${COMP_SRCS}
                        INCLUDE_DIRS "." "./include"
                        REQUIRES micro_babel_event lvgl__lvgl
)

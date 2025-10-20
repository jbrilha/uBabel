set(srcs "sd_card.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES fatfs lvgl_ui
                       WHOLE_ARCHIVE)

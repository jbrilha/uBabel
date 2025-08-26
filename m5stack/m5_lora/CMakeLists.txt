idf_component_register(SRCS "m5_lora.c"
                        INCLUDE_DIRS "." "../../shared"
                        REQUIRES esp_timer driver
)

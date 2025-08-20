idf_component_register(SRCS "m5_lora.cpp"
                        INCLUDE_DIRS "." "../../shared"
                        REQUIRES radiolib esp_timer driver
)

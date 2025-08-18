idf_component_register(SRCS "esp_lora.cpp"
                        INCLUDE_DIRS "." "../../shared"
                        REQUIRES radiolib esp_timer driver
)

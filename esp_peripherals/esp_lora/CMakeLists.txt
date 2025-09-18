idf_component_register(SRCS "esp_lora.c" "esp_lora_receiver.c" "esp_lora_sender.c"
                        INCLUDE_DIRS "." "./include" "../../shared"
                        REQUIRES esp_timer driver spi_manager micro_babel_event
)

add_library(lora INTERFACE)

set(LORA_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/lora.c
  ${CMAKE_CURRENT_LIST_DIR}/sx126x.c
  ${CMAKE_CURRENT_LIST_DIR}/sx127x.c
  )

target_sources(lora INTERFACE
  ${LORA_SOURCES}
)

target_include_directories(lora INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(lora INTERFACE pico_stdlib hardware_i2c hardware_spi)

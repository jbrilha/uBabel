add_library(dht11 INTERFACE)

set(DHT11_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/dht11.c
  )

target_sources(dht11 INTERFACE
  ${DHT11_SOURCES}
)

target_include_directories(dht11 INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(dht11 INTERFACE pico_stdlib gpio_hal time_hal)

add_library(pcf8574 INTERFACE)

set(PCF8574_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/pcf8574.c
  )

target_sources(pcf8574 INTERFACE
  ${PCF8574_SOURCES}
)

target_include_directories(pcf8574 INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(pcf8574 INTERFACE pico_stdlib hardware_i2c)

add_library(lcd16x2 INTERFACE)

set(LCD16x2_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/lcd16x2.c
  )

target_sources(lcd16x2 INTERFACE
  ${LCD16x2_SOURCES}
)

target_include_directories(lcd16x2 INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(lcd16x2 INTERFACE pico_stdlib hardware_i2c)

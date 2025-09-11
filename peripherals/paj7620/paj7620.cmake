add_library(paj7620 INTERFACE)

set(PAJ7620_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/paj7620.c
  )

target_sources(paj7620 INTERFACE
  ${PAJ7620_SOURCES}
)

target_include_directories(paj7620 INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(paj7620 INTERFACE pico_stdlib hardware_i2c)

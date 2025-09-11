add_library(tca9548 INTERFACE)

set(TCA9548_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/tca9548.c
  )

target_sources(tca9548 INTERFACE
  ${TCA9548_SOURCES}
)

target_include_directories(tca9548 INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(tca9548 INTERFACE pico_stdlib hardware_i2c)

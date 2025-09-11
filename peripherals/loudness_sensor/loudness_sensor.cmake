add_library(loudness_sensor INTERFACE)

set(LOUDNESS_SENSOR_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/loudness_sensor.c
  )

target_sources(loudness_sensor INTERFACE
  ${LOUDNESS_SENSOR_SOURCES}
)

target_include_directories(loudness_sensor INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(loudness_sensor INTERFACE pico_stdlib adc_hal)

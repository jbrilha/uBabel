add_library(bmp280 INTERFACE)

set(BMP280_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/bmp280.c
  )

target_sources(bmp280 INTERFACE
  ${BMP280_SOURCES}
)

target_include_directories(bmp280 INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(bmp280 INTERFACE pico_stdlib hardware_i2c)

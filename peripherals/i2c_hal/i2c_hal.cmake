add_library(i2c_hal INTERFACE)

set(I2C_HAL_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/i2c_hal_pico.c
  )

target_sources(i2c_hal INTERFACE
  ${I2C_HAL_SOURCES}
)

target_include_directories(i2c_hal INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(i2c_hal INTERFACE pico_stdlib hardware_i2c)

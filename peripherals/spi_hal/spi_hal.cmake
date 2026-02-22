add_library(spi_hal INTERFACE)

set(SPI_HAL_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/spi_hal_pico.c
  )

target_sources(spi_hal INTERFACE
  ${SPI_HAL_SOURCES}
)

target_include_directories(spi_hal INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(spi_hal INTERFACE pico_stdlib hardware_spi)

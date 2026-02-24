add_library(gpio_hal INTERFACE)

set(GPIO_HAL_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/gpio_hal_pico.c
  )

target_sources(gpio_hal INTERFACE
  ${GPIO_HAL_SOURCES}
)

target_include_directories(gpio_hal INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(gpio_hal INTERFACE pico_stdlib hardware_gpio)

add_library(time_hal INTERFACE)

set(GPIO_HAL_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/time_hal_pico.c
  )

target_sources(time_hal INTERFACE
  ${GPIO_HAL_SOURCES}
)

target_include_directories(time_hal INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(time_hal INTERFACE pico_stdlib)

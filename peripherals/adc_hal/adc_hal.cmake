add_library(adc_hal INTERFACE)

set(ADC_HAL_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/adc_hal.c
  )

target_sources(adc_hal INTERFACE
  ${ADC_HAL_SOURCES}
)

target_include_directories(adc_hal INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(adc_hal INTERFACE pico_stdlib hardware_adc)

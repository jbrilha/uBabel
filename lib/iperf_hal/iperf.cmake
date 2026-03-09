add_library(iperf_hal INTERFACE)

set(IPERF_HAL_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/iperf_hal_pico.c
  )

target_sources(iperf_hal INTERFACE
  ${IPERF_HAL_SOURCES}
)

target_include_directories(iperf_hal INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(iperf_hal INTERFACE pico_stdlib micro_babel_network)

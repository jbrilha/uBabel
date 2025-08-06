add_library(pico_buttons INTERFACE)

target_sources(pico_buttons INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/pico_buttons.c
)

target_include_directories(pico_buttons INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(pico_buttons INTERFACE pico_stdlib hardware_pio micro_babel_event)

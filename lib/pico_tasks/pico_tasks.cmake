add_library(pico_tasks INTERFACE)

target_sources(pico_tasks INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/pico_tasks.c
)

target_include_directories(pico_tasks INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(pico_tasks INTERFACE pico_stdlib hardware_pio micro_babel_event)

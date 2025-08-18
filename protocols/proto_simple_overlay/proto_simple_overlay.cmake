add_library(proto_simple_overlay INTERFACE)

target_sources(proto_simple_overlay INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/proto_simple_overlay.c
)

target_include_directories(proto_simple_overlay INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(proto_simple_overlay INTERFACE micro_babel_common_events micro_babel_event micro_babel_comm_manager)

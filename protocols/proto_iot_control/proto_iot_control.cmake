add_library(proto_iot_control INTERFACE)

target_sources(proto_iot_control INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/proto_iot_control.c
)

target_include_directories(proto_iot_control INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(proto_iot_control INTERFACE micro_babel_common_events micro_babel_event micro_babel_comm_manager)

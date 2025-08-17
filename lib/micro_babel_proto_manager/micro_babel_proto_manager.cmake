add_library(micro_babel_proto_manager INTERFACE)

set(BABEL_PROTO_MANAGER_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/proto_manager.c
  )

target_sources(micro_babel_proto_manager INTERFACE
  ${BABEL_PROTO_MANAGER_SOURCES}
)

target_include_directories(micro_babel_proto_manager INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(micro_babel_proto_manager INTERFACE FreeRTOS-Kernel-Heap4)
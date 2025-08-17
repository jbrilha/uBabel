add_library(micro_babel_network INTERFACE)

set(BABEL_NETWORK_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/network_manager.c
  ${CMAKE_CURRENT_LIST_DIR}/network_config.c
  ${CMAKE_CURRENT_LIST_DIR}/network_platform.c
  )

target_sources(micro_babel_network INTERFACE
  ${BABEL_NETWORK_SOURCES}
)

target_include_directories(micro_babel_network INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(micro_babel_network INTERFACE micro_babel_proto_manager)
if(NOT TARGET micro_babel_proto_manager)
  include(${CMAKE_CURRENT_LIST_DIR}/../micro_babel_proto_manager/micro_babel_proto_manager.cmake)
endif()

add_library(micro_babel_network INTERFACE)

set(BABEL_NETWORK_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/network_manager.c
  ${CMAKE_CURRENT_LIST_DIR}/network_config.c
  ${CMAKE_CURRENT_LIST_DIR}/network_hal.c
  )

target_sources(micro_babel_network INTERFACE
  ${BABEL_NETWORK_SOURCES}
)

target_include_directories(micro_babel_network INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(micro_babel_network INTERFACE micro_babel_proto_manager)

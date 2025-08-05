add_library(babel_network INTERFACE)

set(BABEL_NETWORK_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/network_manager.c
  ${CMAKE_CURRENT_LIST_DIR}/network_config.c
  )

target_sources(babel_network INTERFACE
  ${BABEL_NETWORK_SOURCES}
)

target_include_directories(babel_network INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(babel_network INTERFACE pico_stdlib)

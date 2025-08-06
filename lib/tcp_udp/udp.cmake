add_library(udp INTERFACE)

set(UDP_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/udp.c
  )

target_sources(udp INTERFACE
  ${UDP_SOURCES}
)

target_include_directories(udp INTERFACE ${CMAKE_CURRENT_LIST_DIR})

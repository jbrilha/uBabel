add_library(tcp INTERFACE)

set(TCP_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/tcp.c
  )

target_sources(tcp INTERFACE
  ${TCP_SOURCES}
)

target_include_directories(tcp INTERFACE ${CMAKE_CURRENT_LIST_DIR})

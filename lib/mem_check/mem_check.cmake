add_library(mem_check INTERFACE)

set(MEM_CHECK_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/mem_check.c
  )

target_sources(mem_check INTERFACE
  ${MEM_CHECK_SOURCES}
)

target_include_directories(mem_check INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(pico_scroll INTERFACE pico_stdlib pico_malloc)

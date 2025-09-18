add_library(app INTERFACE)

set(APP_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/app.c
  )

target_sources(app INTERFACE
  ${APP_SOURCES}
)

target_include_directories(app INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(app INTERFACE pico_stdlib pico_malloc)

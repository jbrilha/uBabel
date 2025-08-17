add_library(micro_babel_event INTERFACE)

set(BABEL_EVENT_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/event_dispatcher.c
  ${CMAKE_CURRENT_LIST_DIR}/event.c
  )

target_sources(micro_babel_event INTERFACE
  ${BABEL_EVENT_SOURCES}
)

target_include_directories(micro_babel_event INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(micro_babel_event INTERFACE micro_babel_proto_manager)
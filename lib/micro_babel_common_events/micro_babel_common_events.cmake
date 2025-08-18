if(NOT TARGET micro_babel_event)
  include(${CMAKE_CURRENT_LIST_DIR}/../micro_babel_event/micro_babel_event.cmake)
endif()

if(NOT TARGET micro_babel_comm_manager)
  include(${CMAKE_CURRENT_LIST_DIR}/../micro_babel_comm_/micro_babel_comm.cmake)
endif()

add_library(micro_babel_common_events INTERFACE)

set(BABEL_EVENT_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/common_events.c
  )

target_sources(micro_babel_common_events INTERFACE
  ${BABEL_EVENT_SOURCES}
)

target_include_directories(micro_babel_common_events INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(micro_babel_common_events INTERFACE micro_babel_event micro_babel_comm_manager)
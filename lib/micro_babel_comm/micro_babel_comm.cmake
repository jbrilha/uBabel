add_library(micro_babel_comm_manager INTERFACE)

target_sources(micro_babel_comm_manager INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/comm_manager.c
  ${CMAKE_CURRENT_LIST_DIR}/message_parse.c
)

target_include_directories(micro_babel_comm_manager INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(micro_babel_comm_manager INTERFACE FreeRTOS-Kernel-Heap4 pico_cyw43_arch_lwip_sys_freertos micro_babel_proto_manager micro_babel_event pico_stdlib)

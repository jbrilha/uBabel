add_library(micro_babel_proto_discovery INTERFACE)

target_sources(micro_babel_proto_discovery INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/proto_discovery.c
)

target_include_directories(micro_babel_proto_discovery INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Pull in pico libraries that we need
target_link_libraries(micro_babel_proto_discovery INTERFACE FreeRTOS-Kernel-Heap4 pico_cyw43_arch_lwip_sys_freertos micro_babel_event)

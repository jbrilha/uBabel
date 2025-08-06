add_executable(${PROJECT_NAME}
    freertos_test.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_async_context_freertos
    FreeRTOS-Kernel-Heap4
    pico_cyw43_arch_lwip_sys_freertos
    # pico_cyw43_arch_lwip_threadsafe_background
    pico_stdlib
    pico_scroll
    pico_unicorn
    micro_babel_network
    micro_babel_event
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})

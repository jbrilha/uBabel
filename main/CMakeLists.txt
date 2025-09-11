if(DEFINED BUILD_PICO)
    add_executable(${PROJECT_NAME}
        pico_main.c
    )

    target_sources(${PROJECT_NAME} PRIVATE
        pico_scroll_wrapper.cpp
        pico_unicorn_wrapper.cpp
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        pico_async_context_freertos
        FreeRTOS-Kernel-Heap4
        pico_cyw43_arch_lwip_sys_freertos
        pico_stdlib
        pico_scroll
        pico_unicorn
        pico_buttons
        micro_babel_network
        micro_babel_event
        micro_babel_comm_manager
        micro_babel_common_events
        tcp
        udp
        pico_touch_screen
        mem_check
        proto_simple_overlay
        proto_iot_control
        gpio_hal
        adc_hal
        i2c_hal
        dht11
        loudness_sensor
        tca9548
        paj7620
    )

    pico_enable_stdio_usb(${PROJECT_NAME} 1)
    pico_enable_stdio_uart(${PROJECT_NAME} 0)

    pico_add_extra_outputs(${PROJECT_NAME})
else()
    if(DEFINED M5STACK_CORE_BASIC)
        set(ESP_SRCS m5_main.c)
    else()
        set(ESP_SRCS esp32_main.c)
    endif()

    idf_component_register(SRCS ${ESP_SRCS}
                    INCLUDE_DIRS "." "../shared")
endif()

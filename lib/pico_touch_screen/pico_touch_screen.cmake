add_library(pico_touch_screen INTERFACE)

set(PICO_TOUCH_SCREEN_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/ili9341.c
    ${CMAKE_CURRENT_LIST_DIR}/msp2807_calibration.c
    ${CMAKE_CURRENT_LIST_DIR}/msp2807_touch.c
    ${CMAKE_CURRENT_LIST_DIR}/pico_touch_screen.c
    ${CMAKE_CURRENT_LIST_DIR}/lcd_assert.c
)

target_sources(pico_touch_screen INTERFACE
    ${PICO_TOUCH_SCREEN_SOURCES}
)

target_include_directories(pico_touch_screen INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(pico_touch_screen INTERFACE
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_timer
    hardware_clocks
    pico_sync
)

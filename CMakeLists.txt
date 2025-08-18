cmake_minimum_required(VERSION 3.25)

set(PROJECT_NAME micro-babel)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

if(DEFINED BUILD_PICO)
    add_compile_definitions(BUILD_PICO=1)

    set(PICO_BOARD pico2_w)

    include(pico_sdk_import.cmake)
    include(FreeRTOS_Kernel_import.cmake)

    project(${PROJECT_NAME} C CXX ASM)

    pico_sdk_init()

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/shared)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/protocols)

    add_subdirectory(lib)
    add_subdirectory(common)
    add_subdirectory(main)
    add_subdirectory(protocols)
else()
    add_compile_definitions(BUILD_ESP32=1)
    if(DEFINED M5STACK_CORE_BASIC)
        add_compile_definitions(M5STACK_CORE_BASIC=1)
    endif()

    include($ENV{IDF_PATH}/tools/cmake/project.cmake)

    project(${PROJECT_NAME})
endif()

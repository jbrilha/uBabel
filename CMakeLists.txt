cmake_minimum_required(VERSION 3.25)

set(PICO_BOARD pico_w)

set(PROJECT_NAME micro-babel)

include(pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)

project(${PROJECT_NAME} C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

add_subdirectory(lib)
add_subdirectory(common)
add_subdirectory(src)

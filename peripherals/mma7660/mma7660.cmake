add_library(mma7660 INTERFACE)

set(MMA7660_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/mma7660.c
  )

target_sources(mma7660 INTERFACE
  ${MMA7660_SOURCES}
)

target_include_directories(mma7660 INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(mma7660 INTERFACE pico_stdlib hardware_i2c)

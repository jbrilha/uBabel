if(DEFINED BUILD_PICO)
    include(app.cmake)
else()
    idf_component_register(SRCS "app.c"
                          INCLUDE_DIRS "." "../../shared"
    )
endif()

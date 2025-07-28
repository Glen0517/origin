project(pipewire-dft)

add_library(pipewire-dft SHARED
    dft.c
)

target_include_directories(pipewire-dft
    PRIVATE
        ${PIPEWIRE_INCLUDE_DIRS}
        ${SPA_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../system_log
)

target_link_libraries(pipewire-dft
    PRIVATE
        pipewire-0.3
        spa-0.2
        pthread
        pipewire-system-log
)

install(TARGETS pipewire-dft
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/pipewire-0.3/
)
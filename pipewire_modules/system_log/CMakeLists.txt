project(pipewire-system-log)

add_library(pipewire-system-log SHARED
    system_log.c
)

target_include_directories(pipewire-system-log
    PRIVATE
        ${PIPEWIRE_INCLUDE_DIRS}
        ${SPA_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pipewire-system-log
    PRIVATE
        pipewire-0.3
        spa-0.2
        pthread
)

install(TARGETS pipewire-system-log
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/pipewire-0.3/
)
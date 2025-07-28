project(pipewire-ota-update)

add_library(pipewire-ota-update SHARED
    ota_update.c
)

target_include_directories(pipewire-ota-update
    PRIVATE
        ${PIPEWIRE_INCLUDE_DIRS}
        ${SPA_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../system_log
)

target_link_libraries(pipewire-ota-update
    PRIVATE
        pipewire-0.3
        spa-0.2
        pthread
        pipewire-system-log
)

install(TARGETS pipewire-ota-update
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/pipewire-0.3/
)
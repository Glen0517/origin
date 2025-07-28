cmake_minimum_required(VERSION 3.16)
project(pipewire-bluez VERSION 1.0.0 LANGUAGES C)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../system_log)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES bluez_audio.c)

# Create shared library
add_library(pipewire-bluez SHARED ${SOURCES})

# Find dependencies
pkg_check_modules(BLUEZ REQUIRED bluez)
pkg_check_modules(DBUS REQUIRED dbus-1)
find_package(PipeWire 0.3 REQUIRED)

# Link libraries
target_link_libraries(pipewire-bluez PRIVATE
    PipeWire::PipeWire
    ${BLUEZ_LIBRARIES}
    ${DBUS_LIBRARIES}
    pthread
    pipewire-system-log
)

target_include_directories(pipewire-bluez PRIVATE
    ${BLUEZ_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
)

# Install library
install(TARGETS pipewire-bluez
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/pipewire-0.3/
)
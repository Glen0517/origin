cmake_minimum_required(VERSION 3.16)
project(audio_framework VERSION 1.0.0 LANGUAGES C CXX)

# Set C/C++ standards
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Include directories
include_directories(include)

# Source files
file(GLOB SOURCES "src/*.c")

# Find required libraries
find_package(ALSA REQUIRED)
find_package(PipeWire 0.3 REQUIRED)
find_package(Systemd REQUIRED)

# Kernel module configuration
option(BUILD_KERNEL_MODULE "Build kernel module" ON)
if(BUILD_KERNEL_MODULE)
    add_subdirectory(kernel)
endif()

# Audio processing library
add_subdirectory(audio_processing)

# Routing logic
add_subdirectory(routing)

# Create shared library
add_library(audio_framework SHARED ${SOURCES})

target_link_libraries(audio_framework PRIVATE
    ALSA::ALSA
    PipeWire::PipeWire
    audio_processing
    routing
)

# Install library
install(TARGETS audio_sdk
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(DIRECTORY include/ DESTINATION include/audio_sdk)

# Examples
add_subdirectory(examples)

# Tests
enable_testing()
add_subdirectory(tests)
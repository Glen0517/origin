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
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)
include_directories(${DBUS_INCLUDE_DIRS})

# Kernel module configuration
option(BUILD_KERNEL_MODULE "Build kernel module" ON)
if(BUILD_KERNEL_MODULE)
    add_subdirectory(kernel)
endif()

# Audio processing library
add_subdirectory(audio_processing)

# Routing logic
add_subdirectory(routing)

# 添加模块管理器
add_library(module_manager STATIC src/module_manager.c)
target_include_directories(module_manager PUBLIC include)
 target_link_libraries(module_manager PUBLIC dl pthread)

# 动态模块构建配置
function(add_audio_module MODULE_NAME MODULE_DIR)
    add_library(${MODULE_NAME} SHARED ${MODULE_DIR}/${MODULE_NAME}.c)
    set_target_properties(${MODULE_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/pipewire_modules/${MODULE_NAME}
        PREFIX "lib"
        SUFFIX ".so"
    )
    target_include_directories(${MODULE_NAME} PUBLIC include)
    target_link_libraries(${MODULE_NAME} PUBLIC module_manager audio_framework)
    install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/audio_modules)
endfunction()

# 添加现有模块的动态构建
add_audio_module(system_log pipewire_modules/system_log)
add_audio_module(alsa_plugin pipewire_modules/alsa)
add_audio_module(bluez_audio pipewire_modules/bluez)
add_audio_module(dft pipewire_modules/dft)
add_audio_module(ota_update pipewire_modules/ota_update)

# Create shared library
add_library(audio_framework SHARED ${SOURCES})

target_link_libraries(audio_framework PRIVATE
    ALSA::ALSA
    PipeWire::PipeWire
    audio_processing
    routing
    ${DBUS_LIBRARIES}
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

add_subdirectory(pipewire_modules/system_log)
add_subdirectory(pipewire_modules/dft)
add_subdirectory(pipewire_modules/ota_update)
add_subdirectory(pipewire_modules/bluez)

# 链接模块管理器到主框架
 target_link_libraries(audio_framework PUBLIC module_manager)
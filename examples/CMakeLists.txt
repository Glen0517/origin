# Build examples

# Simple playback example
add_executable(simple_playback simple_playback.c)

target_link_libraries(simple_playback PRIVATE audio_sdk m)

target_include_directories(simple_playback PRIVATE ../include)

# Install examples
install(TARGETS simple_playback
    RUNTIME DESTINATION bin/examples
)
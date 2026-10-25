# Header-only generated-audio sources (tone, sweep, melody) behind a tiny AudioSource
# interface. No hardware, no Bluetooth: bluetooth_a2dp consumes it, an I2S/PWM player could too.
add_library(pico_toolset_audio_synth INTERFACE)
# mus_player.cpp (the MUS-format song player) is compiled into the consumer; the generators and
# SampleMixer stay header-only.
target_sources(pico_toolset_audio_synth INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/mus_player.cpp)
target_include_directories(pico_toolset_audio_synth INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_features(pico_toolset_audio_synth INTERFACE cxx_std_20)

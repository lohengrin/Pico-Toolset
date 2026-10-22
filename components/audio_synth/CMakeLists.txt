# Header-only generated-audio sources (tone, sweep, melody) behind a tiny AudioSource
# interface. No hardware, no Bluetooth: bluetooth_a2dp consumes it, an I2S/PWM player could too.
add_library(pico_toolset_audio_synth INTERFACE)
target_include_directories(pico_toolset_audio_synth INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_features(pico_toolset_audio_synth INTERFACE cxx_std_20)

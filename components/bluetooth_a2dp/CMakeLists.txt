# A2DP source (playback only): generated audio to a Bluetooth headset/speaker. Optional on top
# of pico_toolset_bluetooth -- nothing here is compiled or sized unless this target is linked.
# Needs BTstack Classic (PICO_TOOLSET_BLUETOOTH_CLASSIC=ON, see components/bluetooth).
if(NOT PICO_TOOLSET_BLUETOOTH_CLASSIC)
    message(FATAL_ERROR "pico_toolset_bluetooth_a2dp needs PICO_TOOLSET_BLUETOOTH_CLASSIC=ON")
endif()
if(NOT TARGET pico_toolset_audio_synth)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../audio_synth ${CMAKE_BINARY_DIR}/_toolset/audio_synth)
endif()

add_library(pico_toolset_bluetooth_a2dp INTERFACE)
target_sources(pico_toolset_bluetooth_a2dp INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth_a2dp.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/a2dp_quality.cpp
)
target_include_directories(pico_toolset_bluetooth_a2dp INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_bluetooth_a2dp INTERFACE
    pico_toolset_bluetooth
    pico_toolset_audio_synth
    pico_btstack_sbc_encoder
)
# Switches on the AVDTP/AVRCP/SDP pool sizes in btstack_config.h (builds without A2DP pay nothing).
target_compile_definitions(pico_toolset_bluetooth_a2dp INTERFACE PICO_TOOLSET_BLUETOOTH_A2DP=1)
target_compile_features(pico_toolset_bluetooth_a2dp INTERFACE cxx_std_20)

if(PICO_CYW43_SUPPORTED AND (TARGET pico_toolset_https_client OR PICO_TOOLSET_BLUETOOTH_NO_WIFI))
    add_executable(bluetooth_a2dp_example example/bluetooth_a2dp_example.cpp)
    target_link_libraries(bluetooth_a2dp_example PRIVATE pico_toolset_bluetooth_a2dp)
    # Bonding writes the shared store from core 0; this example runs nothing on core 1.
    target_compile_definitions(bluetooth_a2dp_example PRIVATE PICO_FLASH_ASSUME_CORE1_SAFE=1)
    if(TARGET pico_toolset_https_client AND NOT PICO_TOOLSET_BLUETOOTH_NO_WIFI)
        target_link_libraries(bluetooth_a2dp_example PRIVATE pico_toolset_https_client)
    endif()
    pico_enable_stdio_usb(bluetooth_a2dp_example ON)
    pico_add_extra_outputs(bluetooth_a2dp_example)
endif()

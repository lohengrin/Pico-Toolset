# BLE HID host (keyboard/mouse/gamepad). INTERFACE library with sources for
# the same reason as pico_toolset_bluetooth: the SDK's BTstack/cyw43 targets
# add INTERFACE sources that must compile once, in the final executable. See
# docs/components/bluetooth-hid.md for the flash-safety requirements of
# bonding (PICO_FLASH_ASSUME_CORE1_SAFE when core 1 runs RAM-only code).
# GamepadState lives in components/hid_input (shared with usb_hid).
if(NOT TARGET pico_toolset_hid_input)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../hid_input ${CMAKE_BINARY_DIR}/_toolset/hid_input)
endif()

add_library(pico_toolset_bluetooth_hid INTERFACE)
target_sources(pico_toolset_bluetooth_hid INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth_hid.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/hid_report.cpp
)
target_include_directories(pico_toolset_bluetooth_hid INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src   # hid_report.h (internal)
)
target_link_libraries(pico_toolset_bluetooth_hid INTERFACE
    pico_toolset_bluetooth
    pico_toolset_hid_input
    pico_btstack_flash_bank
)
target_compile_features(pico_toolset_bluetooth_hid INTERFACE cxx_std_20)

if(PICO_CYW43_SUPPORTED AND TARGET pico_toolset_https_client AND NOT PICO_TOOLSET_BLUETOOTH_NO_WIFI)
    add_executable(bluetooth_hid_example example/bluetooth_hid_example.cpp)
    target_link_libraries(bluetooth_hid_example PRIVATE
        pico_toolset_bluetooth_hid
        pico_toolset_https_client
    )
    # This example runs no code on core 1, so flash writes (bonding) need no lockout.
    target_compile_definitions(bluetooth_hid_example PRIVATE PICO_FLASH_ASSUME_CORE1_SAFE=1)
    pico_enable_stdio_usb(bluetooth_hid_example ON)
    pico_add_extra_outputs(bluetooth_hid_example)
endif()

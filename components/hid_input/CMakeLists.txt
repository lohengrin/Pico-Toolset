# Header-only types shared by every HID input transport (USB host, Bluetooth
# HID): the unified GamepadState and its button bits. No hardware, no deps.
add_library(pico_toolset_hid_input INTERFACE)
target_include_directories(pico_toolset_hid_input INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_compile_features(pico_toolset_hid_input INTERFACE cxx_std_20)

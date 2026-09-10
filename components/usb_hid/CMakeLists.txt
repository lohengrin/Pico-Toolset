# USB HID host component: TinyUSB host stack (rhport 1, Pico-PIO-USB HCD)
# plus the Pico SDK's vendored TinyUSB (rhport 0 stays a CDC device). See
# cmake/pico_pio_usb.cmake for how `pio_usb` and `tinyusb_host` are provided.

if(NOT TARGET pico_pio_usb OR NOT TARGET tinyusb_host)
    message(FATAL_ERROR
        "pico_toolset_usb_hid requires pico_pio_usb and tinyusb_host; "
        "enable PICO_TOOLSET_BUILD_USB_HID via the top-level project "
        "and make sure cmake/pico_pio_usb.cmake ran."
    )
endif()

add_library(pico_toolset_usb_hid STATIC
    src/usb_hid_host.cpp
    src/usb_hid_keymap.cpp
)
target_include_directories(pico_toolset_usb_hid PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    # Our tusb_config.h is found here; it must precede pico_stdio_usb's
    # (which is #if'd empty once tinyusb_host is linked) on the include path.
    ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(pico_toolset_usb_hid PUBLIC
    pico_pio_usb
    tinyusb_host
)
target_link_libraries(pico_toolset_usb_hid PRIVATE
    pico_stdlib
    hardware_sync
    pico_multicore
)
target_compile_features(pico_toolset_usb_hid PUBLIC cxx_std_20)

add_executable(usb_hid_example example/usb_hid_example.cpp)
target_include_directories(usb_hid_example PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(usb_hid_example PRIVATE
    pico_toolset_usb_hid
)
# Linking tinyusb_host defines LIB_TINYUSB_HOST, which the SDK headers use to
# silently turn OFF stdio_usb defaults that this app still needs. Restore
# them (see TOM6809's pico/CMakeLists.txt for the full derivation):
#   - own device descriptors instead of none,
#   - stdio_usb_init() still calls tusb_init() for rhport 0,
#   - the background IRQ still pumps tud_task(), and
#   - picotool reset via the RPI vendor interface is preserved.
target_compile_definitions(usb_hid_example PRIVATE
    PICO_STDIO_USB_USE_DEFAULT_DESCRIPTORS=1
    PICO_STDIO_USB_ENABLE_TINYUSB_INIT=1
    PICO_STDIO_USB_ENABLE_IRQ_BACKGROUND_TASK=1
    PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1
    PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=2
    PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR=1
    PICO_ENABLE_USB_RESET_VIA_BAUD_RATE=1
)
pico_enable_stdio_usb(usb_hid_example ON)
pico_add_extra_outputs(usb_hid_example)
# USB composite device (CDC serial + MSC mass storage) over pico-sdk's
# TinyUSB device stack. See README.md.
add_library(pico_toolset_usb_composite STATIC
    src/usb_composite.cpp
)
target_include_directories(pico_toolset_usb_composite PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include  # pico_toolset/usb_composite.h + tusb_config.h
)
target_link_libraries(pico_toolset_usb_composite PUBLIC
    pico_stdlib
    pico_unique_id
    tinyusb_device
    pico_usb_reset
)
# picotool support: vendor "reset" interface (reboot to BOOTSEL / to flash
# without touching the BOOTSEL button) plus the 1200-baud CDC touch. Public
# because pico_usb_reset's source is compiled into the final executable.
# PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF must match ITF_RESET in usb_composite.cpp.
target_compile_definitions(pico_toolset_usb_composite PUBLIC
    PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1
    PICO_USB_RESET_SUPPORT_RESET_TO_BOOTSEL=1
    PICO_USB_RESET_SUPPORT_RESET_TO_FLASH_BOOT=1
    PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR=1
    PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=3
    PICO_USB_RESET_INCLUDE_DEFAULT_APP_DRIVER_CB=1
    PICO_ENABLE_USB_RESET_VIA_BAUD_RATE=1
)
target_compile_features(pico_toolset_usb_composite PUBLIC cxx_std_20)

add_executable(usb_composite_example example/usb_composite_example.cpp)
target_link_libraries(usb_composite_example PRIVATE pico_toolset_usb_composite)
pico_enable_stdio_usb(usb_composite_example OFF)
pico_enable_stdio_uart(usb_composite_example OFF)
pico_add_extra_outputs(usb_composite_example)

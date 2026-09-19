# USB composite device (CDC serial + MSC mass storage + picotool reset
# interface) over pico-sdk's TinyUSB device stack.
#
# Two variants of the same sources, because TinyUSB allows exactly one
# tusb_config.h per build and it must match what the *final executable's*
# TinyUSB core is compiled with (see usb_hid's tusb_config.h for the ABI
# hazard):
#   pico_toolset_usb_composite      device only (own config/tusb_config.h)
#   pico_toolset_usb_composite_hid  built against pico_toolset_usb_hid's
#                                   config, for firmware that also runs the
#                                   PIO-USB host (keyboard/mouse). Only
#                                   defined when that component was added
#                                   first. Link exactly one of the two.
function(pico_toolset_usb_composite_variant name config_dir)
    add_library(${name} STATIC ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/usb_composite.cpp)
    target_include_directories(${name} PUBLIC
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include
        ${config_dir}
    )
    target_link_libraries(${name} PUBLIC pico_stdlib pico_unique_id tinyusb_device pico_usb_reset)
    target_compile_features(${name} PUBLIC cxx_std_20)
    # Values the composite needs whichever tusb_config.h is in use (both
    # configs honour -D overrides).
    target_compile_definitions(${name} PUBLIC
        CFG_TUD_CDC=1
        CFG_TUD_MSC=1
        CFG_TUD_MSC_EP_BUFSIZE=4096
        CFG_TUD_CDC_TX_BUFSIZE=512
        # picotool support: vendor "reset" interface (reboot to BOOTSEL / to
        # flash without the BOOTSEL button) plus the 1200-baud CDC touch.
        # Public because pico_usb_reset's source compiles into the final
        # executable. ITF must match ITF_RESET in usb_composite.cpp (and
        # usb_hid's identical definition).
        PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1
        PICO_USB_RESET_SUPPORT_RESET_TO_BOOTSEL=1
        PICO_USB_RESET_SUPPORT_RESET_TO_FLASH_BOOT=1
        PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR=1
        PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=2
        PICO_USB_RESET_INCLUDE_DEFAULT_APP_DRIVER_CB=1
        PICO_ENABLE_USB_RESET_VIA_BAUD_RATE=1
    )
endfunction()

pico_toolset_usb_composite_variant(pico_toolset_usb_composite ${CMAKE_CURRENT_LIST_DIR}/config)
if(TARGET pico_toolset_usb_hid)
    pico_toolset_usb_composite_variant(pico_toolset_usb_composite_hid ${CMAKE_CURRENT_LIST_DIR}/../usb_hid)
    target_link_libraries(pico_toolset_usb_composite_hid PUBLIC pico_toolset_usb_hid)
endif()

add_executable(usb_composite_example example/usb_composite_example.cpp)
target_link_libraries(usb_composite_example PRIVATE pico_toolset_usb_composite)
pico_enable_stdio_usb(usb_composite_example OFF)
pico_enable_stdio_uart(usb_composite_example OFF)
pico_add_extra_outputs(usb_composite_example)

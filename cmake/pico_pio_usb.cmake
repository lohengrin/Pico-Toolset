# pico_toolset -- helper to make Pico-PIO-USB the TinyUSB HID *host*.
#
# Two ways to supply Pico-PIO-USB:
#   1. Set PICO_PIO_USB_DIR to a populated Pico-PIO-USB checkout (repo root),
#      e.g. -DPICO_PIO_USB_DIR=/path/to/Pico-PIO-USB
#   2. Let FetchContent clone it (needs a git checkout on the build machine).
#
# Design (mirrors the toolset's proven RP2350 build):
#   * Pico-PIO-USB's current master provides ONE INTERFACE library target,
#     `pico_pio_usb` (PIO programs + host/device/CRC sources). There is no
#     `tinyusb_host` inside it -- that lives in the Pico SDK's own vendored
#     TinyUSB (src/rp2_common/tinyusb), which defines `tinyusb_host`.
#   * pio_usb.c switches its standalone API vs TinyUSB HCD/DCD callbacks on
#     PIO_USB_USE_TINYUSB; set it INTERFACE on the library itself so every
#     consumer builds it as a host-controller-driver.
#   * The HCD glue (hcd_pio_usb.c) comes from the SDK's vendored TinyUSB
#     (PICO_TINYUSB_PATH, only set if the submodule is present) and is added
#     as an INTERFACE source so consumers compile it.
#
# After this file runs, `pio_usb` and `tinyusb_host` targets both exist (or
# it fatal-errors with a hint).

if(NOT PICO_TOOLSET_BUILD_USB_HID)
    return()
endif()

if(NOT TARGET tinyusb_host)
    message(FATAL_ERROR
        "pico_toolset: USB HID needs the Pico SDK's vendored TinyUSB host "
        "support, but the SDK's tinyusb submodule has not been initialized. "
        "Run: git -C \${PICO_SDK_PATH} submodule update --init lib/tinyusb"
        " (or set PICO_TINYUSB_PATH to a TinyUSB checkout)."
    )
endif()

if(TARGET pico_pio_usb)
    return()
endif()

if(NOT DEFINED PICO_PIO_USB_DIR)
    include(FetchContent)
    FetchContent_Declare(
        pico_pio_usb
        GIT_REPOSITORY https://github.com/sekigon-gonnoc/Pico-PIO-USB.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(pico_pio_usb)
else()
    add_subdirectory(${PICO_PIO_USB_DIR} ${CMAKE_BINARY_DIR}/pico_pio_usb)
endif()

if(NOT TARGET pico_pio_usb)
    message(FATAL_ERROR
        "pico_toolset: Pico-PIO-USB configured but no pico_pio_usb target "
        "(is PICO_PIO_USB_DIR pointing at a current checkout of "
        "github.com/sekigon-gonnoc/Pico-PIO-USB?)."
    )
endif()

# Build pio_usb as a TinyUSB HCD rather than its standalone host API.
target_compile_definitions(pico_pio_usb INTERFACE PIO_USB_USE_TINYUSB)

# The HCD glue registering pio_usb as a TinyUSB host-controller port. Added
# INTERFACE so any consumer linking pico_pio_usb compiles it too.
if(PICO_TINYUSB_PATH)
    target_sources(pico_pio_usb INTERFACE
        ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
    )
else()
    message(FATAL_ERROR
        "pico_toolset: PICO_TINYUSB_PATH is not set; cannot build the "
        "Pico-PIO-USB host-controller driver."
    )
endif()

message(STATUS "pico_toolset: Pico-PIO-USB ready (pio_usb + tinyusb_host)")
cmake_minimum_required(VERSION 3.20)

# --- pico-sdk location ------------------------------------------------------
# pico_sdk_import must be included BEFORE project() so its pre-load
# platform/toolchain setup runs before CMake snapshots the (cross-)compilers.
# Uses PICO_SDK_PATH (cache, or env) when available; otherwise the SDK fetches
# the pinned release from git.
if(NOT PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "" FORCE)
    else()
        message(WARNING "PICO_SDK_PATH not set -- pico_sdk_import will fetch "
                        "pico-sdk from git (may take a while on first configure).")
        set(PICO_SDK_FETCH_FROM_GIT ON CACHE BOOL "" FORCE)
        set(PICO_SDK_FETCH_FROM_GIT_TAG "2.3.1" CACHE STRING "" FORCE)
    endif()
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_toolset VERSION 1.0.0 LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The fetch path fully initializes the SDK as a subproject already; the
# PICO_SDK_PATH path needs an explicit pico_sdk_init().
if(NOT TARGET _pico_sdk_inclusion_marker)
    pico_sdk_init()
endif()

option(PICO_TOOLSET_BUILD_SSD1306  "Build SSD1306 OLED driver"            ON)
option(PICO_TOOLSET_BUILD_ILI9486  "Build ILI9486 SPI LCD driver"         ON)
option(PICO_TOOLSET_BUILD_PSRAM    "Build PSRAM driver (RP2350 only)"     ON)
option(PICO_TOOLSET_BUILD_USB_HID  "Build PIO-USB HID host"               ON)
option(PICO_TOOLSET_BUILD_SCREEN   "Build Screen abstraction"             ON)
# Screen backend selector.
option(PICO_TOOLSET_SCREEN_PIMORONI "Use Pimoroni PicoGraphics backend"   OFF)

if(PICO_TOOLSET_BUILD_SSD1306)
    add_subdirectory(components/ssd1306)
endif()
if(PICO_TOOLSET_BUILD_ILI9486)
    add_subdirectory(components/ili9486)
endif()
if(PICO_TOOLSET_BUILD_PSRAM)
    add_subdirectory(components/psram)
endif()
if(PICO_TOOLSET_BUILD_USB_HID)
    include(cmake/pico_pio_usb.cmake)
    add_subdirectory(components/usb_hid)
endif()
if(PICO_TOOLSET_BUILD_SCREEN)
    add_subdirectory(libs/screen)
endif()

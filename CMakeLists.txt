# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD waveshare_rp2350_pizero CACHE STRING "Board type")

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
option(PICO_TOOLSET_BUILD_ST7789   "Build ST7789 SPI LCD driver"          ON)
option(PICO_TOOLSET_BUILD_XPT2046  "Build XPT2046 resistive touch driver" ON)
option(PICO_TOOLSET_BUILD_PSRAM    "Build PSRAM driver (RP2350 only)"     ON)
option(PICO_TOOLSET_BUILD_USB_HID  "Build PIO-USB HID host"               ON)
# OFF by default (unlike the other components): needs pico-extras' pico_audio_i2s
# target to already exist, which needs pico_extras_import.cmake run before
# project() by the consumer -- this component can't set that up itself. See
# the README's "I2S audio (pico-extras)" section.
option(PICO_TOOLSET_BUILD_I2S_AUDIO "Build I2S audio output (needs pico-extras)" OFF)
option(PICO_TOOLSET_BUILD_SDCARD   "Build SD card (FatFs/pico_fatfs) driver" ON)
option(PICO_TOOLSET_BUILD_RESET_BUTTONS "Build debounced-buttons + tagged-watchdog-reboot helper" ON)
option(PICO_TOOLSET_BUILD_DVI_HDMI "Build PIO-based DVI/HDMI video (+ optional digital audio)" ON)
option(PICO_TOOLSET_BUILD_FAULT_HANDLER "Build hard-fault handler (watchdog-scratch crash reporting)" ON)
option(PICO_TOOLSET_BUILD_SCREEN   "Build Screen abstraction"             ON)
# Screen backend selector.
option(PICO_TOOLSET_SCREEN_PIMORONI "Use Pimoroni PicoGraphics backend"   OFF)

if(PICO_TOOLSET_BUILD_SSD1306)
    add_subdirectory(components/ssd1306)
endif()
if(PICO_TOOLSET_BUILD_ILI9486)
    add_subdirectory(components/ili9486)
endif()
if(PICO_TOOLSET_BUILD_ST7789)
    add_subdirectory(components/st7789)
endif()
if(PICO_TOOLSET_BUILD_XPT2046)
    add_subdirectory(components/xpt2046)
endif()
if(PICO_TOOLSET_BUILD_PSRAM)
    add_subdirectory(components/psram)
endif()
if(PICO_TOOLSET_BUILD_USB_HID)
    include(cmake/pico_pio_usb.cmake)
    add_subdirectory(components/usb_hid)
endif()
if(PICO_TOOLSET_BUILD_I2S_AUDIO)
    add_subdirectory(components/i2s_audio)
endif()
if(PICO_TOOLSET_BUILD_SDCARD)
    include(cmake/pico_fatfs.cmake)
    add_subdirectory(components/sdcard)
endif()
if(PICO_TOOLSET_BUILD_RESET_BUTTONS)
    add_subdirectory(components/reset_buttons)
endif()
if(PICO_TOOLSET_BUILD_DVI_HDMI)
    add_subdirectory(components/dvi_hdmi)
endif()
if(PICO_TOOLSET_BUILD_FAULT_HANDLER)
    add_subdirectory(libs/fault_handler)
endif()
if(PICO_TOOLSET_BUILD_SCREEN)
    add_subdirectory(libs/screen)
endif()

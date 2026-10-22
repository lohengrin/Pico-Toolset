# Standalone consumer project: Pimoroni Presto full-integration example
# (ST7701 LCD + FT6236 touch + uSD + PSRAM + WS2812 + piezo + QW/ST I2C). See
# docs/boards/pimoroni-presto.md. pico-sdk ships the board header
# (pimoroni_presto.h), so nothing is vendored in boards/.
cmake_minimum_required(VERSION 3.20)

set(TOOLSET_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

set(PICO_BOARD pimoroni_presto CACHE STRING "" FORCE)

# Force PICO_PLATFORM too -- see examples/waveshare_pizero/CMakeLists.txt.
set(PICO_PLATFORM rp2350 CACHE STRING "" FORCE)

if(NOT PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "" FORCE)
    else()
        message(FATAL_ERROR "PICO_SDK_PATH must be set (env or -D) to build this example.")
    endif()
endif()
include(${TOOLSET_ROOT}/pico_sdk_import.cmake)

# Bluetooth Classic (older gamepads/keyboards) next to BLE. Must be set before the bluetooth
# component is added below.
option(PRESTO_BT_CLASSIC "Support Bluetooth Classic HID devices (older pads) in addition to BLE" ON)
option(PRESTO_BT_A2DP "Bluetooth audio: stream generated sound to a headset (A2DP source; implies Classic)" OFF)
if(PRESTO_BT_A2DP)
    set(PRESTO_BT_CLASSIC ON CACHE BOOL "" FORCE)
endif()
set(PICO_TOOLSET_BLUETOOTH_CLASSIC ${PRESTO_BT_CLASSIC} CACHE BOOL "" FORCE)

# One mbedtls config per build (lwipopts.h from pico_toolset_net_config turns
# lwIP's TLS layer on, so Wi-Fi/Bluetooth builds need mbedtls linked).
# Must be set before pico_sdk_init().
include(${TOOLSET_ROOT}/cmake/pico_toolset_tls_config.cmake)

project(presto_example C CXX ASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
pico_sdk_init()

set(EXAMPLE_OUTPUT_NAME "presto" CACHE STRING "Base name for the produced executable/uf2")
option(PRESTO_FULL_RES "Drive the panel at 480x480 instead of 240x240 (pixel-doubled)" OFF)
option(PRESTO_PALETTE "8-bit palette framebuffer (half the SRAM, RGB332 colours) instead of RGB565" OFF)

# cmake/pico_fatfs.cmake gates on this option (see components/sdcard).
set(PICO_TOOLSET_BUILD_SDCARD ON)
include(${TOOLSET_ROOT}/cmake/pico_fatfs.cmake)

add_subdirectory(${TOOLSET_ROOT}/components/driver_interfaces ${CMAKE_BINARY_DIR}/_toolset/driver_interfaces)
add_subdirectory(${TOOLSET_ROOT}/components/psram   ${CMAKE_BINARY_DIR}/_toolset/psram)
add_subdirectory(${TOOLSET_ROOT}/components/sdcard  ${CMAKE_BINARY_DIR}/_toolset/sdcard)
add_subdirectory(${TOOLSET_ROOT}/components/st7701  ${CMAKE_BINARY_DIR}/_toolset/st7701)
add_subdirectory(${TOOLSET_ROOT}/components/ft6236  ${CMAKE_BINARY_DIR}/_toolset/ft6236)
add_subdirectory(${TOOLSET_ROOT}/components/ws2812  ${CMAKE_BINARY_DIR}/_toolset/ws2812)
add_subdirectory(${TOOLSET_ROOT}/components/piezo   ${CMAKE_BINARY_DIR}/_toolset/piezo)
add_subdirectory(${TOOLSET_ROOT}/components/qwstpad ${CMAKE_BINARY_DIR}/_toolset/qwstpad)
add_subdirectory(${TOOLSET_ROOT}/components/bme280  ${CMAKE_BINARY_DIR}/_toolset/bme280)
add_subdirectory(${TOOLSET_ROOT}/components/ltr559  ${CMAKE_BINARY_DIR}/_toolset/ltr559)
add_subdirectory(${TOOLSET_ROOT}/components/lsm6ds3 ${CMAKE_BINARY_DIR}/_toolset/lsm6ds3)
add_subdirectory(${TOOLSET_ROOT}/components/net_config   ${CMAKE_BINARY_DIR}/_toolset/net_config)
add_subdirectory(${TOOLSET_ROOT}/components/wifi         ${CMAKE_BINARY_DIR}/_toolset/wifi)
add_subdirectory(${TOOLSET_ROOT}/components/https_client ${CMAKE_BINARY_DIR}/_toolset/https_client)
add_subdirectory(${TOOLSET_ROOT}/components/bluetooth    ${CMAKE_BINARY_DIR}/_toolset/bluetooth)
add_subdirectory(${TOOLSET_ROOT}/components/bluetooth_hid ${CMAKE_BINARY_DIR}/_toolset/bluetooth_hid)
if(PRESTO_BT_A2DP)
    add_subdirectory(${TOOLSET_ROOT}/components/audio_synth    ${CMAKE_BINARY_DIR}/_toolset/audio_synth)
    add_subdirectory(${TOOLSET_ROOT}/components/bluetooth_a2dp ${CMAKE_BINARY_DIR}/_toolset/bluetooth_a2dp)
endif()
add_subdirectory(${TOOLSET_ROOT}/libs/screen        ${CMAKE_BINARY_DIR}/_toolset/screen)

add_executable(${EXAMPLE_OUTPUT_NAME} main.cpp)
target_link_libraries(${EXAMPLE_OUTPUT_NAME} PRIVATE
    pico_toolset_psram
    pico_toolset_sdcard
    pico_toolset_st7701
    pico_toolset_ft6236
    pico_toolset_ws2812
    pico_toolset_piezo
    pico_toolset_screen
    pico_toolset_qwstpad
    pico_toolset_bme280
    pico_toolset_ltr559
    pico_toolset_lsm6ds3
    pico_stdlib
    pico_multicore
    hardware_i2c
)
# Wi-Fi credentials: a directory containing a git-ignored config.local.h that
# defines WIFI_SSID / WIFI_PASSWORD (never commit one). Also read from the
# environment, so the superbuild picks it up:
#   PRESTO_WIFI_CONFIG_DIR=~/Dev/PicoADSB/src cmake -S examples -B examples-build
set(PRESTO_WIFI_CONFIG_DIR "$ENV{PRESTO_WIFI_CONFIG_DIR}" CACHE PATH "Directory holding config.local.h with WIFI_SSID/WIFI_PASSWORD")
if(PRESTO_WIFI_CONFIG_DIR)
    target_include_directories(${EXAMPLE_OUTPUT_NAME} PRIVATE ${PRESTO_WIFI_CONFIG_DIR})
endif()
# Debug aid for Bluetooth pairing problems: dump every HCI/L2CAP/SMP packet to the
# USB serial port. The log is paused while scanning and runs from the moment a
# connection is started, so it holds just the pairing exchange.
option(PRESTO_BT_HCI_DUMP "Log all Bluetooth HCI packets to the serial console" OFF)
if(PRESTO_BT_HCI_DUMP)
    target_compile_definitions(${EXAMPLE_OUTPUT_NAME} PRIVATE WANT_HCI_DUMP=1)
endif()
if(PRESTO_PALETTE)
    target_compile_definitions(${EXAMPLE_OUTPUT_NAME} PRIVATE PRESTO_PALETTE=1)
endif()
if(PRESTO_FULL_RES)
    target_compile_definitions(${EXAMPLE_OUTPUT_NAME} PRIVATE PRESTO_FULL_RES=1)
endif()
if(PRESTO_FULL_RES AND NOT PRESTO_PALETTE)
    # A 450 KB RGB565 scanout buffer leaves too little SRAM for the radio
    # stack (lwIP + mbedtls + BTstack overflow RAM by several KB), so that
    # combination drops Wi-Fi and BLE. Full-res + PRESTO_PALETTE keeps them.
    target_compile_definitions(${EXAMPLE_OUTPUT_NAME} PRIVATE PRESTO_NO_RADIO=1)
else()
    target_link_libraries(${EXAMPLE_OUTPUT_NAME} PRIVATE
        pico_toolset_wifi
        pico_toolset_bluetooth
        pico_toolset_bluetooth_hid
        pico_toolset_https_client # only to satisfy lwIP's TLS layer (see above)
    )
    if(PRESTO_BT_A2DP)
        target_link_libraries(${EXAMPLE_OUTPUT_NAME} PRIVATE pico_toolset_bluetooth_a2dp)
        target_compile_definitions(${EXAMPLE_OUTPUT_NAME} PRIVATE PRESTO_BT_A2DP=1)
        # The SBC encoder runs in the BTstack IRQ context on the core 0 stack (default 2 KB).
        target_compile_definitions(${EXAMPLE_OUTPUT_NAME} PRIVATE PICO_STACK_SIZE=0x1000)
    endif()
    # Bonding writes flash from core 0; core 1 only runs RAM-resident code (LCD
    # scanout interrupts + core1_idle()), so the SDK need not stop it. See
    # docs/components/bluetooth-hid.md.
    target_compile_definitions(${EXAMPLE_OUTPUT_NAME} PRIVATE PICO_FLASH_ASSUME_CORE1_SAFE=1)
endif()
pico_enable_stdio_usb(${EXAMPLE_OUTPUT_NAME} ON)
pico_add_extra_outputs(${EXAMPLE_OUTPUT_NAME})

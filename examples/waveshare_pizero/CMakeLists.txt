# Standalone consumer project: Waveshare RP2350-PiZero base combination
# (HDMI + PSRAM (present-or-not) + sdcard + USB HID). See
# boards/waveshare_rp2350_pizero.md. Single board (RP2350-only) so, unlike
# examples/pico_dv, PICO_BOARD isn't parameterized here.
cmake_minimum_required(VERSION 3.20)

set(TOOLSET_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

set(PICO_BOARD waveshare_rp2350_pizero CACHE STRING "" FORCE)
list(APPEND PICO_BOARD_HEADER_DIRS ${TOOLSET_ROOT}/boards)

# Force PICO_PLATFORM too -- pico-sdk only skips its PICO_PLATFORM-from-
# environment defaulting when the cache variable is already set, so a
# leftover `export PICO_PLATFORM=...` from a different board's shell env
# script (e.g. rp2350-riscv) otherwise collides with this RP2350-only board
# and pico-sdk aborts with an incompatibility error.
set(PICO_PLATFORM rp2350 CACHE STRING "" FORCE)

if(NOT PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "" FORCE)
    else()
        message(FATAL_ERROR "PICO_SDK_PATH must be set (env or -D) to build this example.")
    endif()
endif()
include(${TOOLSET_ROOT}/pico_sdk_import.cmake)

project(waveshare_pizero_example C CXX ASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
pico_sdk_init()

set(EXAMPLE_OUTPUT_NAME "waveshare_pizero" CACHE STRING "Base name for the produced executable/uf2")

# cmake/pico_fatfs.cmake and pico_pio_usb.cmake gate on these options (see
# components/sdcard, components/usb_hid).
set(PICO_TOOLSET_BUILD_SDCARD ON)
set(PICO_TOOLSET_BUILD_USB_HID ON)
include(${TOOLSET_ROOT}/cmake/pico_fatfs.cmake)
include(${TOOLSET_ROOT}/cmake/pico_pio_usb.cmake)

add_subdirectory(${TOOLSET_ROOT}/components/dvi_hdmi ${CMAKE_BINARY_DIR}/_toolset/dvi_hdmi)
add_subdirectory(${TOOLSET_ROOT}/components/psram    ${CMAKE_BINARY_DIR}/_toolset/psram)
add_subdirectory(${TOOLSET_ROOT}/components/sdcard   ${CMAKE_BINARY_DIR}/_toolset/sdcard)
add_subdirectory(${TOOLSET_ROOT}/components/usb_hid  ${CMAKE_BINARY_DIR}/_toolset/usb_hid)

add_executable(${EXAMPLE_OUTPUT_NAME} main.cpp)
target_include_directories(${EXAMPLE_OUTPUT_NAME} PRIVATE
    ${TOOLSET_ROOT}/components/usb_hid # tusb_config.h, see that component's own example
)
target_link_libraries(${EXAMPLE_OUTPUT_NAME} PRIVATE
    pico_toolset_dvi_hdmi
    pico_toolset_psram
    pico_toolset_sdcard
    pico_toolset_usb_hid
    pico_stdlib
    pico_multicore
)
pico_enable_stdio_usb(${EXAMPLE_OUTPUT_NAME} ON)
pico_add_extra_outputs(${EXAMPLE_OUTPUT_NAME})

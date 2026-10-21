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

project(presto_example C CXX ASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
pico_sdk_init()

set(EXAMPLE_OUTPUT_NAME "presto" CACHE STRING "Base name for the produced executable/uf2")
option(PRESTO_FULL_RES "Drive the panel at 480x480 instead of 240x240 (pixel-doubled)" OFF)

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
    pico_stdlib
    pico_multicore
    hardware_i2c
)
if(PRESTO_FULL_RES)
    target_compile_definitions(${EXAMPLE_OUTPUT_NAME} PRIVATE PRESTO_FULL_RES=1)
endif()
pico_enable_stdio_usb(${EXAMPLE_OUTPUT_NAME} ON)
pico_add_extra_outputs(${EXAMPLE_OUTPUT_NAME})

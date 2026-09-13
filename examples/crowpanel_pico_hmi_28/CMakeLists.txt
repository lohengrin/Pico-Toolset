# Standalone consumer project: Elecrow CrowPanel PICO HMI 2.8" (ST7789 SPI
# display + XPT2046 touch + uSD card, all sharing SPI1 with separate CS
# lines). See ../../boards/crowpanel_pico_hmi_28.md. Plain RP2040 "pico"
# board -- no custom PICO_BOARD header needed for this board.
cmake_minimum_required(VERSION 3.20)

set(TOOLSET_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

set(PICO_BOARD pico CACHE STRING "" FORCE)

if(NOT PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "" FORCE)
    else()
        message(FATAL_ERROR "PICO_SDK_PATH must be set (env or -D) to build this example.")
    endif()
endif()
include(${TOOLSET_ROOT}/pico_sdk_import.cmake)

project(crowpanel_pico_hmi_28_example C CXX ASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
pico_sdk_init()

set(EXAMPLE_OUTPUT_NAME "crowpanel_pico_hmi_28" CACHE STRING "Base name for the produced executable/uf2")

set(PICO_TOOLSET_BUILD_SDCARD ON)
include(${TOOLSET_ROOT}/cmake/pico_fatfs.cmake)

add_subdirectory(${TOOLSET_ROOT}/components/driver_interfaces ${CMAKE_BINARY_DIR}/_toolset/driver_interfaces)
add_subdirectory(${TOOLSET_ROOT}/components/st7789  ${CMAKE_BINARY_DIR}/_toolset/st7789)
add_subdirectory(${TOOLSET_ROOT}/components/xpt2046 ${CMAKE_BINARY_DIR}/_toolset/xpt2046)
add_subdirectory(${TOOLSET_ROOT}/components/sdcard  ${CMAKE_BINARY_DIR}/_toolset/sdcard)
add_subdirectory(${TOOLSET_ROOT}/libs/screen        ${CMAKE_BINARY_DIR}/_toolset/screen)

add_executable(${EXAMPLE_OUTPUT_NAME} main.cpp)
target_link_libraries(${EXAMPLE_OUTPUT_NAME} PRIVATE
    pico_toolset_st7789
    pico_toolset_xpt2046
    pico_toolset_sdcard
    pico_toolset_screen
    pico_stdlib
)
pico_enable_stdio_usb(${EXAMPLE_OUTPUT_NAME} ON)
pico_add_extra_outputs(${EXAMPLE_OUTPUT_NAME})

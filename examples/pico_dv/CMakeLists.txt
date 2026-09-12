# Standalone consumer project: "pico-dv" carrier board (sdcard + HDMI +
# I2S audio + 3 buttons) full-combination example. See boards/pico_dv_*.md.
#
# Builds for Pico 1 / Pico 1 W / Pico 2 by PICO_BOARD alone (same carrier,
# same code) -- set via -DPICO_BOARD=pico|pico_w|pico2, default `pico`.
# Normally driven by the examples/ superbuild (see examples/CMakeLists.txt),
# which builds all three variants in one `cmake --build` with no flags
# needed; this file also stands alone for one-off/manual builds.
cmake_minimum_required(VERSION 3.20)

set(TOOLSET_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

if(NOT PICO_BOARD)
    set(PICO_BOARD pico CACHE STRING "pico | pico_w | pico2")
endif()
set(PICO_BOARD ${PICO_BOARD} CACHE STRING "pico | pico_w | pico2" FORCE)

# Force PICO_PLATFORM to match PICO_BOARD, regardless of any PICO_PLATFORM
# already exported in the shell -- pico-sdk's own PICO_PLATFORM-from-
# environment defaulting only backs off when the PICO_PLATFORM *cache*
# variable is already set, so a common pico-sdk habit like
# `export PICO_BOARD=pico2; export PICO_PLATFORM=rp2350` (e.g. a saved
# env.sh for a different board) otherwise collides with the pico/pico_w
# (rp2040) legs here and pico-sdk aborts with "PICO_PLATFORM is specified
# to be 'rp2350', but PICO_BOARD='pico' ... is incompatible."
if(PICO_BOARD STREQUAL "pico2")
    set(PICO_PLATFORM rp2350 CACHE STRING "" FORCE)
else()
    set(PICO_PLATFORM rp2040 CACHE STRING "" FORCE)
endif()

if(NOT PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "" FORCE)
    else()
        message(FATAL_ERROR "PICO_SDK_PATH must be set (env or -D) to build this example.")
    endif()
endif()
include(${TOOLSET_ROOT}/pico_sdk_import.cmake)

if(NOT PICO_EXTRAS_PATH)
    if(DEFINED ENV{PICO_EXTRAS_PATH})
        set(PICO_EXTRAS_PATH $ENV{PICO_EXTRAS_PATH} CACHE PATH "" FORCE)
    else()
        message(FATAL_ERROR
            "PICO_EXTRAS_PATH must be set (env or -D) -- this example needs "
            "pico-extras' pico_audio_i2s for i2s_audio. See "
            "github.com/raspberrypi/pico-extras."
        )
    endif()
endif()
include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

project(pico_dv_example C CXX ASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
pico_sdk_init()

# Base name for the produced executable/.uf2 -- the superbuild overrides
# this per PICO_BOARD pass so each output is named for its configuration
# (pico_dv_pico1.uf2, pico_dv_pico1w.uf2, pico_dv_pico2.uf2).
set(EXAMPLE_OUTPUT_NAME "pico_dv" CACHE STRING "Base name for the produced executable/uf2")

# cmake/pico_fatfs.cmake gates on this option (see components/sdcard).
set(PICO_TOOLSET_BUILD_SDCARD ON)
include(${TOOLSET_ROOT}/cmake/pico_fatfs.cmake)

add_subdirectory(${TOOLSET_ROOT}/components/dvi_hdmi      ${CMAKE_BINARY_DIR}/_toolset/dvi_hdmi)
add_subdirectory(${TOOLSET_ROOT}/components/sdcard        ${CMAKE_BINARY_DIR}/_toolset/sdcard)
add_subdirectory(${TOOLSET_ROOT}/components/i2s_audio     ${CMAKE_BINARY_DIR}/_toolset/i2s_audio)
add_subdirectory(${TOOLSET_ROOT}/components/reset_buttons ${CMAKE_BINARY_DIR}/_toolset/reset_buttons)

add_executable(${EXAMPLE_OUTPUT_NAME} main.cpp)
target_link_libraries(${EXAMPLE_OUTPUT_NAME} PRIVATE
    pico_toolset_dvi_hdmi
    pico_toolset_sdcard
    pico_toolset_i2s_audio
    pico_toolset_reset_buttons
    pico_stdlib
    pico_multicore
)
pico_enable_stdio_usb(${EXAMPLE_OUTPUT_NAME} ON)
pico_add_extra_outputs(${EXAMPLE_OUTPUT_NAME})

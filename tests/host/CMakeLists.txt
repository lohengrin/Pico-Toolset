# Host (PC) unit tests for the hardware-independent parts of the Bluetooth stack:
#   cmake -S tests/host -B tests-build && cmake --build tests-build && ctest --test-dir tests-build
# BTstack's own HID parser is compiled from the pico-sdk checkout (PICO_SDK_PATH or -DBTSTACK_DIR=...).
cmake_minimum_required(VERSION 3.16)
project(pico_toolset_host_tests CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TOOLSET_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
if(NOT BTSTACK_DIR)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(BTSTACK_DIR $ENV{PICO_SDK_PATH}/lib/btstack)
    else()
        message(FATAL_ERROR "Set PICO_SDK_PATH or -DBTSTACK_DIR=<btstack checkout>")
    endif()
endif()

add_executable(bluetooth_host_tests
    test_main.cpp
    ${TOOLSET_ROOT}/components/audio_synth/src/mus_player.cpp
    ${TOOLSET_ROOT}/components/bluetooth_a2dp/src/a2dp_quality.cpp
    ${TOOLSET_ROOT}/components/bluetooth_hid/src/hid_report.cpp
    ${BTSTACK_DIR}/src/btstack_hid_parser.c
    ${BTSTACK_DIR}/src/btstack_util.c
)
target_include_directories(bluetooth_host_tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}                       # btstack_config.h (empty)
    ${TOOLSET_ROOT}/components/audio_synth/include
    ${TOOLSET_ROOT}/components/bluetooth_a2dp/include
    ${TOOLSET_ROOT}/components/bluetooth_hid/include
    ${TOOLSET_ROOT}/components/bluetooth_hid/src
    ${TOOLSET_ROOT}/components/hid_input/include
    ${TOOLSET_ROOT}/components/st7701/include
    ${BTSTACK_DIR}/src
    ${BTSTACK_DIR}/platform/embedded
)
target_compile_options(bluetooth_host_tests PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME bluetooth_host_tests COMMAND bluetooth_host_tests)

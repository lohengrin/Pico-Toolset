# Host (PC) smoke test of the LVGL view of the Bluetooth page, on a headless LVGL display:
#   cmake -S tests/host_lvgl -B tests-lvgl-build -DLVGL_DIR=<lvgl v9 checkout> && cmake --build tests-lvgl-build && ctest --test-dir tests-lvgl-build
# (any LVGL v9 checkout works, e.g. the one a PicoBoot build fetched: build-presto/_deps/lvgl-src)
cmake_minimum_required(VERSION 3.16)
project(pico_toolset_host_tests_lvgl CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT LVGL_DIR)
    message(FATAL_ERROR "Pass -DLVGL_DIR=<lvgl v9 checkout>")
endif()
set(TOOLSET_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h CACHE STRING "" FORCE)
add_subdirectory(${LVGL_DIR} lvgl EXCLUDE_FROM_ALL)

add_executable(bluetooth_lvgl_host_tests
    test_bluetooth_ui_lvgl.cpp
    ${TOOLSET_ROOT}/libs/bluetooth_ui/src/bluetooth_devices.cpp
    ${TOOLSET_ROOT}/libs/bluetooth_ui/src/bluetooth_page_controller.cpp
    ${TOOLSET_ROOT}/libs/bluetooth_ui/src/bluetooth_page_text.cpp
    ${TOOLSET_ROOT}/libs/bluetooth_ui_lvgl/src/bluetooth_page_lvgl.cpp
)
target_include_directories(bluetooth_lvgl_host_tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../host   # fake_bt_backend.h
    ${TOOLSET_ROOT}/libs/bluetooth_ui/include
    ${TOOLSET_ROOT}/libs/bluetooth_ui_lvgl/include
)
target_link_libraries(bluetooth_lvgl_host_tests PRIVATE lvgl)
target_compile_options(bluetooth_lvgl_host_tests PRIVATE -Wall -Wextra)
enable_testing()
add_test(NAME bluetooth_lvgl_host_tests COMMAND bluetooth_lvgl_host_tests)

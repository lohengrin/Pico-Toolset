# LVGL v9 display + touch bridge over the DisplayPanel/TouchPanel interfaces.
# Requires the `lvgl` target (see cmake/pico_lvgl.cmake).
if(NOT TARGET lvgl)
    message(FATAL_ERROR "pico_toolset_lvgl_display requires the lvgl target; include cmake/pico_lvgl.cmake first (with LV_CONF_PATH set).")
endif()
add_library(pico_toolset_lvgl_display STATIC src/lvgl_display.cpp src/lvgl_gpio_keys.cpp)
target_include_directories(pico_toolset_lvgl_display PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(pico_toolset_lvgl_display PUBLIC
    pico_stdlib
    hardware_gpio
    pico_toolset_driver_interfaces
    lvgl::lvgl
)
target_compile_features(pico_toolset_lvgl_display PUBLIC cxx_std_20)

# Optional: LVGL input devices for the USB HID host (keyboard/mouse/gamepad).
# Separate target so consumers without the HID component don't pull in the
# TinyUSB host stack.
if(TARGET pico_toolset_usb_hid)
    add_library(pico_toolset_lvgl_hid STATIC src/lvgl_hid.cpp)
    target_link_libraries(pico_toolset_lvgl_hid PUBLIC pico_toolset_lvgl_display pico_toolset_usb_hid)
endif()

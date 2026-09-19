# LVGL v9 display + touch bridge over the DisplayPanel/TouchPanel interfaces.
# Requires the `lvgl` target (see cmake/pico_lvgl.cmake).
if(NOT TARGET lvgl)
    message(FATAL_ERROR "pico_toolset_lvgl_display requires the lvgl target; include cmake/pico_lvgl.cmake first (with LV_CONF_PATH set).")
endif()
add_library(pico_toolset_lvgl_display STATIC src/lvgl_display.cpp)
target_include_directories(pico_toolset_lvgl_display PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(pico_toolset_lvgl_display PUBLIC
    pico_stdlib
    pico_toolset_driver_interfaces
    lvgl::lvgl
)
target_compile_features(pico_toolset_lvgl_display PUBLIC cxx_std_20)

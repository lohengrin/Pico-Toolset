# LVGL view of the common Bluetooth page: the same BluetoothPageController (libs/bluetooth_ui) drawn
# with LVGL widgets, for applications that already run LVGL. Pick this OR the DisplayDriver view of
# pico_toolset_bluetooth_ui; the logic, texts and screens are shared. Needs the `lvgl` target
# (see cmake/pico_lvgl.cmake). Sources compile into the consumer.
if(NOT TARGET lvgl)
    message(FATAL_ERROR "pico_toolset_bluetooth_ui_lvgl requires the lvgl target; include cmake/pico_lvgl.cmake first.")
endif()
add_library(pico_toolset_bluetooth_ui_lvgl INTERFACE)
target_sources(pico_toolset_bluetooth_ui_lvgl INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth_page_lvgl.cpp)
target_include_directories(pico_toolset_bluetooth_ui_lvgl INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_compile_features(pico_toolset_bluetooth_ui_lvgl INTERFACE cxx_std_20)
target_link_libraries(pico_toolset_bluetooth_ui_lvgl INTERFACE pico_toolset_bluetooth_ui lvgl::lvgl)

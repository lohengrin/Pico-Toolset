# Platform-independent logic of the common Bluetooth page: merging devices from the HID and A2DP
# components, and the page's state machine. Sources compile into the consumer. The adaptors
# bluetooth_ui_hid.h / bluetooth_ui_a2dp.h are header-only: include the ones for the components you link.
add_library(pico_toolset_bluetooth_ui INTERFACE)
target_sources(pico_toolset_bluetooth_ui INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth_devices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth_page_controller.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth_page_layout.cpp
)
target_include_directories(pico_toolset_bluetooth_ui INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_compile_features(pico_toolset_bluetooth_ui INTERFACE cxx_std_20)
if(TARGET pico_toolset_hid_input)
    target_link_libraries(pico_toolset_bluetooth_ui INTERFACE pico_toolset_hid_input)
endif()
if(TARGET pico_toolset_bluetooth_hid)
    target_link_libraries(pico_toolset_bluetooth_ui INTERFACE pico_toolset_bluetooth_hid)
endif()
if(TARGET pico_toolset_bluetooth_a2dp)
    target_link_libraries(pico_toolset_bluetooth_ui INTERFACE pico_toolset_bluetooth_a2dp)
endif()

# The view draws through libs/screen's DisplayDriver; only compiled where that library exists.
if(TARGET pico_toolset_screen)
    target_sources(pico_toolset_bluetooth_ui INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth_page_view.cpp)
    target_sources(pico_toolset_bluetooth_ui INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth_page_input.cpp)
    target_link_libraries(pico_toolset_bluetooth_ui INTERFACE pico_toolset_screen)
endif()

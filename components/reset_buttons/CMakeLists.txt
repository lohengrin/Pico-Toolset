add_library(pico_toolset_reset_buttons STATIC
    src/reset_buttons.cpp
)
target_include_directories(pico_toolset_reset_buttons PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_reset_buttons PUBLIC
    pico_stdlib
)
target_link_libraries(pico_toolset_reset_buttons PRIVATE
    hardware_gpio
    hardware_watchdog
)
target_compile_features(pico_toolset_reset_buttons PUBLIC cxx_std_20)

add_executable(reset_buttons_example example/reset_buttons_example.cpp)
target_link_libraries(reset_buttons_example PRIVATE
    pico_toolset_reset_buttons
)
pico_enable_stdio_usb(reset_buttons_example ON)
pico_add_extra_outputs(reset_buttons_example)

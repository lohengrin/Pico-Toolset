add_library(pico_toolset_rgb_led STATIC
    src/rgb_led.cpp
)
target_include_directories(pico_toolset_rgb_led PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_rgb_led PUBLIC
    pico_stdlib
)
target_link_libraries(pico_toolset_rgb_led PRIVATE
    hardware_gpio
    hardware_pwm
)
target_compile_features(pico_toolset_rgb_led PUBLIC cxx_std_20)

add_executable(rgb_led_example example/rgb_led_example.cpp)
target_link_libraries(rgb_led_example PRIVATE
    pico_toolset_rgb_led
)
pico_add_extra_outputs(rgb_led_example)

add_library(pico_toolset_ssd1306 STATIC
    src/ssd1306.cpp
)
target_include_directories(pico_toolset_ssd1306 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_include_directories(pico_toolset_ssd1306 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)
target_link_libraries(pico_toolset_ssd1306 PUBLIC
    pico_stdlib
    hardware_i2c
)
target_link_libraries(pico_toolset_ssd1306 PRIVATE
    hardware_gpio
)
target_compile_features(pico_toolset_ssd1306 PUBLIC cxx_std_20)

add_executable(ssd1306_example example/ssd1306_example.cpp)
target_link_libraries(ssd1306_example PRIVATE
    pico_toolset_ssd1306
)
pico_enable_stdio_usb(ssd1306_example ON)
pico_add_extra_outputs(ssd1306_example)
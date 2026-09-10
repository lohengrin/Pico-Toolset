add_library(pico_toolset_ili9486 STATIC
    src/ili9486.cpp
)
target_include_directories(pico_toolset_ili9486 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_ili9486 PUBLIC
    pico_stdlib
    hardware_spi
)
target_link_libraries(pico_toolset_ili9486 PRIVATE
    hardware_gpio
    hardware_dma
    hardware_pwm
)
target_compile_features(pico_toolset_ili9486 PUBLIC cxx_std_20)

add_executable(ili9486_example example/ili9486_example.cpp)
target_link_libraries(ili9486_example PRIVATE
    pico_toolset_ili9486
)
pico_enable_stdio_usb(ili9486_example ON)
pico_add_extra_outputs(ili9486_example)
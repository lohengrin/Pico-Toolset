add_library(pico_toolset_st7789 STATIC
    src/st7789.cpp
)
target_include_directories(pico_toolset_st7789 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_st7789 PUBLIC
    pico_stdlib
    hardware_spi
)
target_link_libraries(pico_toolset_st7789 PRIVATE
    hardware_gpio
    hardware_dma
    hardware_pwm
)
target_compile_features(pico_toolset_st7789 PUBLIC cxx_std_20)

add_executable(st7789_example example/st7789_example.cpp)
target_link_libraries(st7789_example PRIVATE
    pico_toolset_st7789
)
pico_enable_stdio_usb(st7789_example ON)
pico_add_extra_outputs(st7789_example)

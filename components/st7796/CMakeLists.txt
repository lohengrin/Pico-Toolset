add_library(pico_toolset_st7796 STATIC
    src/st7796.cpp
)
target_include_directories(pico_toolset_st7796 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_st7796 PUBLIC
    pico_stdlib
    hardware_spi
    pico_toolset_driver_interfaces
)
target_link_libraries(pico_toolset_st7796 PRIVATE
    hardware_gpio
    hardware_dma
    hardware_pwm
)
target_compile_features(pico_toolset_st7796 PUBLIC cxx_std_20)

add_executable(st7796_example example/st7796_example.cpp)
target_link_libraries(st7796_example PRIVATE
    pico_toolset_st7796
)
pico_enable_stdio_usb(st7796_example ON)
pico_add_extra_outputs(st7796_example)

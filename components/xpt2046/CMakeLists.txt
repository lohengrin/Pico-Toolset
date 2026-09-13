add_library(pico_toolset_xpt2046 STATIC
    src/xpt2046.cpp
)
target_include_directories(pico_toolset_xpt2046 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_xpt2046 PUBLIC
    pico_stdlib
    hardware_spi
    pico_toolset_driver_interfaces
)
target_link_libraries(pico_toolset_xpt2046 PRIVATE
    hardware_gpio
)
target_compile_features(pico_toolset_xpt2046 PUBLIC cxx_std_20)

add_executable(xpt2046_example example/xpt2046_example.cpp)
target_link_libraries(xpt2046_example PRIVATE
    pico_toolset_xpt2046
)
pico_enable_stdio_usb(xpt2046_example ON)
pico_add_extra_outputs(xpt2046_example)

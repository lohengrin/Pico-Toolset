add_library(pico_toolset_ft6236 STATIC
    src/ft6236.cpp
)
target_include_directories(pico_toolset_ft6236 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_ft6236 PUBLIC
    pico_stdlib
    hardware_i2c
    pico_toolset_driver_interfaces
)
target_link_libraries(pico_toolset_ft6236 PRIVATE
    hardware_gpio
)
target_compile_features(pico_toolset_ft6236 PUBLIC cxx_std_20)

add_executable(ft6236_example example/ft6236_example.cpp)
target_link_libraries(ft6236_example PRIVATE
    pico_toolset_ft6236
)
pico_enable_stdio_usb(ft6236_example ON)
pico_add_extra_outputs(ft6236_example)

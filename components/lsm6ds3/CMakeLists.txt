add_library(pico_toolset_lsm6ds3 STATIC
    src/lsm6ds3.cpp
)
target_include_directories(pico_toolset_lsm6ds3 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_lsm6ds3 PUBLIC
    pico_stdlib
    hardware_i2c
)
target_compile_features(pico_toolset_lsm6ds3 PUBLIC cxx_std_20)

add_executable(lsm6ds3_example example/lsm6ds3_example.cpp)
target_link_libraries(lsm6ds3_example PRIVATE
    pico_toolset_lsm6ds3
)
pico_enable_stdio_usb(lsm6ds3_example ON)
pico_add_extra_outputs(lsm6ds3_example)

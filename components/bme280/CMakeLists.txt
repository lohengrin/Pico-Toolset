add_library(pico_toolset_bme280 STATIC
    src/bme280.cpp
    src/bosch/bme280.c
)
target_include_directories(pico_toolset_bme280 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_include_directories(pico_toolset_bme280 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)
target_link_libraries(pico_toolset_bme280 PUBLIC
    pico_stdlib
    hardware_i2c
)
target_compile_definitions(pico_toolset_bme280 PRIVATE BME280_FLOAT_ENABLE)
target_compile_features(pico_toolset_bme280 PUBLIC cxx_std_20)

add_executable(bme280_example example/bme280_example.cpp)
target_link_libraries(bme280_example PRIVATE
    pico_toolset_bme280
)
pico_enable_stdio_usb(bme280_example ON)
pico_add_extra_outputs(bme280_example)

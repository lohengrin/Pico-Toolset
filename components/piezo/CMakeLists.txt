add_library(pico_toolset_piezo STATIC
    src/piezo.cpp
)
target_include_directories(pico_toolset_piezo PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_piezo PUBLIC
    pico_stdlib
)
target_link_libraries(pico_toolset_piezo PRIVATE
    hardware_gpio
    hardware_pwm
    hardware_clocks
)
target_compile_features(pico_toolset_piezo PUBLIC cxx_std_20)

add_executable(piezo_example example/piezo_example.cpp)
target_link_libraries(piezo_example PRIVATE
    pico_toolset_piezo
)
pico_add_extra_outputs(piezo_example)

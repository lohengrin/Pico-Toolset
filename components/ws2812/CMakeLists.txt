add_library(pico_toolset_ws2812 STATIC
    src/ws2812.cpp
)
target_include_directories(pico_toolset_ws2812 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_ws2812 PUBLIC
    pico_stdlib
    hardware_pio
)
target_link_libraries(pico_toolset_ws2812 PRIVATE
    hardware_clocks
)
pico_generate_pio_header(pico_toolset_ws2812 ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)
target_compile_features(pico_toolset_ws2812 PUBLIC cxx_std_20)

add_executable(ws2812_example example/ws2812_example.cpp)
target_link_libraries(ws2812_example PRIVATE
    pico_toolset_ws2812
)
pico_add_extra_outputs(ws2812_example)

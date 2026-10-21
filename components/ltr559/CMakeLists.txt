add_library(pico_toolset_ltr559 STATIC
    src/ltr559.cpp
)
target_include_directories(pico_toolset_ltr559 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_ltr559 PUBLIC
    pico_stdlib
    hardware_i2c
)
target_compile_features(pico_toolset_ltr559 PUBLIC cxx_std_20)

add_executable(ltr559_example example/ltr559_example.cpp)
target_link_libraries(ltr559_example PRIVATE
    pico_toolset_ltr559
)
pico_enable_stdio_usb(ltr559_example ON)
pico_add_extra_outputs(ltr559_example)

add_library(pico_toolset_qwstpad STATIC
    src/qwstpad.cpp
)
target_include_directories(pico_toolset_qwstpad PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_qwstpad PUBLIC
    pico_stdlib
    hardware_i2c
    pico_toolset_hid_input   # qwstpad_gamepad.h
)
target_compile_features(pico_toolset_qwstpad PUBLIC cxx_std_20)

add_executable(qwstpad_example example/qwstpad_example.cpp)
target_link_libraries(qwstpad_example PRIVATE
    pico_toolset_qwstpad
)
pico_enable_stdio_usb(qwstpad_example ON)
pico_add_extra_outputs(qwstpad_example)

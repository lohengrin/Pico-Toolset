# USB composite device (CDC serial + MSC mass storage) over pico-sdk's
# TinyUSB device stack. See README.md.
add_library(pico_toolset_usb_composite STATIC
    src/usb_composite.cpp
)
target_include_directories(pico_toolset_usb_composite PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include  # pico_toolset/usb_composite.h + tusb_config.h
)
target_link_libraries(pico_toolset_usb_composite PUBLIC
    pico_stdlib
    pico_unique_id
    tinyusb_device
)
target_compile_features(pico_toolset_usb_composite PUBLIC cxx_std_20)

add_executable(usb_composite_example example/usb_composite_example.cpp)
target_link_libraries(usb_composite_example PRIVATE pico_toolset_usb_composite)
pico_enable_stdio_usb(usb_composite_example OFF)
pico_enable_stdio_uart(usb_composite_example OFF)
pico_add_extra_outputs(usb_composite_example)

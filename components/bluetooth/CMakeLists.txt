# INTERFACE library with sources, like pico_toolset_wifi: the SDK's
# cyw43_arch/BTstack targets contribute INTERFACE sources, so a STATIC library
# linking them would compile the whole stack twice. Needs a CYW43 board
# (PICO_CYW43_SUPPORTED); shares the build's single lwipopts.h with
# pico_toolset_net_config, which enables lwIP's TLS layer, so the final
# executable must also link mbedtls: link pico_toolset_https_client too (and
# include cmake/pico_toolset_tls_config.cmake before pico_sdk_init()).
add_library(pico_toolset_bluetooth INTERFACE)
target_sources(pico_toolset_bluetooth INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth.cpp
)
target_include_directories(pico_toolset_bluetooth INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/config   # btstack_config.h -- one per build
)
target_link_libraries(pico_toolset_bluetooth INTERFACE
    pico_stdlib
    pico_toolset_net_config
    pico_cyw43_arch_lwip_threadsafe_background
    pico_btstack_ble
    pico_btstack_cyw43
)
target_compile_features(pico_toolset_bluetooth INTERFACE cxx_std_20)

if(PICO_CYW43_SUPPORTED AND TARGET pico_toolset_https_client)
    add_executable(bluetooth_example example/bluetooth_example.cpp)
    target_link_libraries(bluetooth_example PRIVATE pico_toolset_bluetooth pico_toolset_https_client)
    pico_enable_stdio_usb(bluetooth_example ON)
    pico_add_extra_outputs(bluetooth_example)
endif()

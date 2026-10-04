# INTERFACE library with sources: pico-sdk's cyw43_arch/lwIP targets add their
# sources as INTERFACE sources, so linking them from a STATIC library would
# compile the whole lwIP/CYW43 stack into it a second time. Compiling our files
# in the final executable avoids the duplicates.
add_library(pico_toolset_wifi INTERFACE)
target_sources(pico_toolset_wifi INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/wifi.cpp
)
target_include_directories(pico_toolset_wifi INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_wifi INTERFACE
    pico_stdlib
    pico_toolset_net_config
    pico_cyw43_arch_lwip_threadsafe_background
)
target_compile_features(pico_toolset_wifi INTERFACE cxx_std_20)

# Small RP2040/RP2350 helpers that several projects kept re-writing: an RAII interrupt-off
# section, the BOOTSEL button as a runtime input, and SRAM-heap headroom queries. No other
# toolset dependencies.
add_library(pico_toolset_rp_util STATIC
    src/bootsel.cpp
    src/sram.cpp
)
target_include_directories(pico_toolset_rp_util PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(pico_toolset_rp_util PUBLIC pico_stdlib)
target_link_libraries(pico_toolset_rp_util PRIVATE hardware_sync)
target_compile_features(pico_toolset_rp_util PUBLIC cxx_std_20)

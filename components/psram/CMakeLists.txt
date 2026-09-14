if(PICO_RP2350)
    add_library(pico_toolset_psram STATIC
        src/psram.cpp
    )
    target_include_directories(pico_toolset_psram PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    )
    target_link_libraries(pico_toolset_psram PUBLIC
        pico_stdlib
        hardware_psram
    )
    target_link_libraries(pico_toolset_psram PRIVATE
        hardware_flash
        hardware_clocks
        pico_flash
        pico_multicore
    )
    target_compile_features(pico_toolset_psram PUBLIC cxx_std_20)

    add_executable(psram_example example/psram_example.cpp)
    target_link_libraries(psram_example PRIVATE
        pico_toolset_psram
    )
    pico_enable_stdio_usb(psram_example ON)
    pico_add_extra_outputs(psram_example)

    # QSPI clock-sweep harness: copy_to_ram is load-bearing, not a build
    # nicety -- the sweep raises clk_sys until flash/PSRAM XIP stops
    # returning usable data, so instructions (and TinyUSB stdio) must run
    # from SRAM for the failing step to be reported instead of crashing the
    # CPU on a bad instruction fetch. See the example's file-top comment.
    add_executable(psram_qspi_sweep_example example/psram_qspi_sweep_example.cpp)
    target_link_libraries(psram_qspi_sweep_example PRIVATE
        pico_toolset_psram
        hardware_clocks
        hardware_vreg
        hardware_xip_cache
    )
    pico_set_binary_type(psram_qspi_sweep_example copy_to_ram)
    pico_enable_stdio_usb(psram_qspi_sweep_example ON)
    pico_add_extra_outputs(psram_qspi_sweep_example)
else()
    message(STATUS "pico_toolset_psram: skipped (RP2350 only)")
endif()
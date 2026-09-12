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
else()
    message(STATUS "pico_toolset_psram: skipped (RP2350 only)")
endif()
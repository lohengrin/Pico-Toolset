if(PICO_RP2350)
    add_library(pico_toolset_st7701 STATIC
        src/st7701.cpp
    )
    target_include_directories(pico_toolset_st7701 PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    )
    target_link_libraries(pico_toolset_st7701 PUBLIC
        pico_stdlib
        hardware_pio
        hardware_spi
    )
    target_link_libraries(pico_toolset_st7701 PRIVATE
        hardware_gpio
        hardware_dma
        hardware_pwm
        hardware_clocks
        hardware_irq
    )
    pico_generate_pio_header(pico_toolset_st7701 ${CMAKE_CURRENT_LIST_DIR}/src/st7701_parallel.pio)
    pico_generate_pio_header(pico_toolset_st7701 ${CMAKE_CURRENT_LIST_DIR}/src/st7701_timing.pio)
    target_compile_features(pico_toolset_st7701 PUBLIC cxx_std_20)

    add_executable(st7701_example example/st7701_example.cpp)
    target_link_libraries(st7701_example PRIVATE
        pico_toolset_st7701
        pico_multicore
    )
    pico_enable_stdio_usb(st7701_example ON)
    pico_add_extra_outputs(st7701_example)
else()
    message(STATUS "pico_toolset_st7701: skipped (RP2350 only)")
endif()

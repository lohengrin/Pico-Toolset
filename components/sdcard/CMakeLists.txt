# SD card component: FatFs R0.15 over elehobica/pico_fatfs. See
# cmake/pico_fatfs.cmake for how the `pico_fatfs` target is provided.

if(NOT TARGET pico_fatfs)
    message(FATAL_ERROR
        "pico_toolset_sdcard requires the pico_fatfs target; enable "
        "PICO_TOOLSET_BUILD_SDCARD via the top-level project and make sure "
        "cmake/pico_fatfs.cmake ran."
    )
endif()

add_library(pico_toolset_sdcard STATIC
    src/sdcard.cpp
)
target_include_directories(pico_toolset_sdcard PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_sdcard PUBLIC
    pico_stdlib
    pico_fatfs
    hardware_pio
    hardware_spi
)
target_compile_features(pico_toolset_sdcard PUBLIC cxx_std_20)

add_executable(sdcard_example example/sdcard_example.cpp)
target_link_libraries(sdcard_example PRIVATE
    pico_toolset_sdcard
)
pico_enable_stdio_usb(sdcard_example ON)
pico_add_extra_outputs(sdcard_example)

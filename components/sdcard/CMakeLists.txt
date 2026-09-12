# SD card component: FatFs R0.15 over elehobica/pico_fatfs. See
# cmake/pico_fatfs.cmake for how the `pico_fatfs` target is provided.

if(NOT TARGET pico_fatfs)
    message(FATAL_ERROR
        "pico_toolset_sdcard requires the pico_fatfs target; enable "
        "PICO_TOOLSET_BUILD_SDCARD via the top-level project and make sure "
        "cmake/pico_fatfs.cmake ran."
    )
endif()

# Overrides newlib's global weak file syscalls (open/read/write/lseek/
# fstat/stat/isatty) to route through FatFs -- a whole-program decision, so
# opt-in rather than always compiled into pico_toolset_sdcard. See
# sdcard_stdio.cpp's header comment and the README's "POSIX/stdio file
# access" section.
option(PICO_TOOLSET_SDCARD_STDIO "Also install POSIX/stdio newlib syscalls (fopen/fread/...) over FatFs" OFF)

set(PICO_TOOLSET_SDCARD_SOURCES src/sdcard.cpp)
if(PICO_TOOLSET_SDCARD_STDIO)
    list(APPEND PICO_TOOLSET_SDCARD_SOURCES src/sdcard_stdio.cpp)
endif()

add_library(pico_toolset_sdcard STATIC
    ${PICO_TOOLSET_SDCARD_SOURCES}
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

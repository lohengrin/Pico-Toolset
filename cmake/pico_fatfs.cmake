# pico_toolset -- helper to make elehobica/pico_fatfs (FatFs R0.15 + PIO/
# native-SPI SD backend) available as the `pico_fatfs` target.
#
# Two ways to supply it:
#   1. Set PICO_FATFS_DIR to a populated pico_fatfs checkout (repo root).
#   2. Let FetchContent clone it (needs a git checkout on the build machine).

if(NOT PICO_TOOLSET_BUILD_SDCARD)
    return()
endif()

if(TARGET pico_fatfs)
    return()
endif()

if(NOT DEFINED PICO_FATFS_DIR)
    include(FetchContent)
    FetchContent_Declare(
        pico_fatfs
        GIT_REPOSITORY https://github.com/elehobica/pico_fatfs.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(pico_fatfs)
else()
    add_subdirectory(${PICO_FATFS_DIR} ${CMAKE_BINARY_DIR}/pico_fatfs)
endif()

if(NOT TARGET pico_fatfs)
    message(FATAL_ERROR
        "pico_toolset: pico_fatfs configured but no pico_fatfs target "
        "(is PICO_FATFS_DIR pointing at a current checkout of "
        "github.com/elehobica/pico_fatfs?)."
    )
endif()

# PICO_PIO_USE_GPIO_BASE -- required whenever any configured SD pin is >=32
# (RP2350B's single 32-pin-wide PIO addressing window otherwise silently
# wraps a >=32 pin number back into 0-31, producing garbage EXECCTRL/PINCTRL
# rather than an error -- confirmed on real hardware as an "SD card times
# out forever" symptom). Set unconditionally, INTERFACE, on `pico_fatfs`
# itself: this is what actually reaches hardware_pio's own compilation for
# the final consuming executable (pico-sdk's hardware_* libraries compile
# their sources per final target, not once as a prebuilt archive, so an
# INTERFACE definition here propagates all the way through) -- harmless for
# boards where every SD pin is <32.
target_compile_definitions(pico_fatfs INTERFACE PICO_PIO_USE_GPIO_BASE=1)

message(STATUS "pico_toolset: pico_fatfs ready")

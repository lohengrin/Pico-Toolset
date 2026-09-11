# I2S audio output component: wraps pico-extras' pico_audio_i2s. That
# library's own pico_extras_import.cmake must run before project(), the same
# constraint pico_sdk_import.cmake has -- this component cannot own that
# setup itself (unlike e.g. the USB HID component fetching Pico-PIO-USB via
# FetchContent after project()), so the consumer must have already imported
# pico-extras and the pico_audio_i2s target must already exist by the time
# this is add_subdirectory()'d. See the toolset README's "I2S audio
# (pico-extras)" section for the exact steps.

if(NOT TARGET pico_audio_i2s)
    message(FATAL_ERROR
        "pico_toolset_i2s_audio requires the pico_audio_i2s target (from "
        "pico-extras) to already exist. Call pico_extras_import.cmake "
        "BEFORE project() -- the same constraint pico_sdk_import.cmake has "
        "-- and link pico_audio_i2s yourself; see the toolset README."
    )
endif()

add_library(pico_toolset_i2s_audio STATIC
    src/i2s_audio.cpp
)
target_include_directories(pico_toolset_i2s_audio PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(pico_toolset_i2s_audio PUBLIC
    pico_stdlib
    pico_audio_i2s
)
target_compile_features(pico_toolset_i2s_audio PUBLIC cxx_std_20)

add_executable(i2s_audio_example example/i2s_audio_example.cpp)
target_link_libraries(i2s_audio_example PRIVATE
    pico_toolset_i2s_audio
)
pico_enable_stdio_usb(i2s_audio_example ON)
pico_add_extra_outputs(i2s_audio_example)

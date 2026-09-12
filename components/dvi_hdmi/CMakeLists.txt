# PIO-based DVI/TMDS serialiser + encoder, optionally carrying HDMI
# data-island digital audio -- see README.md for full provenance. INTERFACE
# library (not a normal static lib) so it can be configured per-consuming-
# target via compile-time defines (DVI_VERTICAL_REPEAT, DVI_MONOCHROME_TMDS,
# etc. -- see dvi_config_defs.h), same rationale the upstream CMakeLists.txt
# gave for the vendored base this carries forward.

option(PICO_TOOLSET_DVI_HDMI_AUDIO
    "Enable HDMI data-island digital audio (CEA-861 InfoFrames/ACR/audio-sample packets)"
    OFF)
option(PICO_TOOLSET_DVI_HDMI_IRQ_STATS
    "Enable a cumulative-microseconds counter around the per-scanline DMA IRQ handler (bring-up/measurement tool)"
    OFF)

add_library(pico_toolset_dvi_hdmi INTERFACE)

target_sources(pico_toolset_dvi_hdmi INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/dvi.c
    ${CMAKE_CURRENT_LIST_DIR}/dvi.h
    ${CMAKE_CURRENT_LIST_DIR}/dvi_config_defs.h
    ${CMAKE_CURRENT_LIST_DIR}/dvi_serialiser.c
    ${CMAKE_CURRENT_LIST_DIR}/dvi_serialiser.h
    ${CMAKE_CURRENT_LIST_DIR}/dvi_timing.c
    ${CMAKE_CURRENT_LIST_DIR}/dvi_timing.h
    ${CMAKE_CURRENT_LIST_DIR}/tmds_encode.S
    ${CMAKE_CURRENT_LIST_DIR}/tmds_encode.c
    ${CMAKE_CURRENT_LIST_DIR}/tmds_encode.h
    ${CMAKE_CURRENT_LIST_DIR}/tmds_table.h
    ${CMAKE_CURRENT_LIST_DIR}/tmds_table_fullres.h
    ${CMAKE_CURRENT_LIST_DIR}/util_queue_u32_inline.h
    )

target_include_directories(pico_toolset_dvi_hdmi INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(pico_toolset_dvi_hdmi INTERFACE
    pico_base_headers
    pico_util
    hardware_dma
    hardware_interp
    hardware_pio
    hardware_pwm
    )

pico_generate_pio_header(pico_toolset_dvi_hdmi ${CMAKE_CURRENT_LIST_DIR}/dvi_serialiser.pio)
pico_generate_pio_header(pico_toolset_dvi_hdmi ${CMAKE_CURRENT_LIST_DIR}/tmds_encode_1bpp.pio)

# HDMI data-island audio: audio_ring.cpp/data_packet.cpp are new files (not
# vendored), so they're additive sources gated behind their own option
# rather than part of the always-on base list above. Off by default: with
# it off, dvi.c/dvi.h/dvi_timing.c compile to byte-identical output to the
# vendored (pure-DVI, video-only) originals -- see dvi_config_defs.h's
# DVI_ENABLE_AUDIO doc comment, the regression firewall this mirrors.
if(PICO_TOOLSET_DVI_HDMI_AUDIO)
    target_sources(pico_toolset_dvi_hdmi INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/audio_ring.cpp
        ${CMAKE_CURRENT_LIST_DIR}/audio_ring.h
        ${CMAKE_CURRENT_LIST_DIR}/data_packet.cpp
        ${CMAKE_CURRENT_LIST_DIR}/data_packet.h
        )
    target_compile_definitions(pico_toolset_dvi_hdmi INTERFACE DVI_ENABLE_AUDIO=1)
endif()

# Core1 IRQ-handler-headroom stats -- a bring-up/measurement tool, not meant
# to ship on by default under any profile. See dvi_config_defs.h's
# DVI_ENABLE_IRQ_STATS doc comment. Independent of PICO_TOOLSET_DVI_HDMI_AUDIO
# (build with audio off for a baseline reading, on for the after reading,
# both with this flag on, to measure exactly what the data-island work
# costs).
if(PICO_TOOLSET_DVI_HDMI_IRQ_STATS)
    target_compile_definitions(pico_toolset_dvi_hdmi INTERFACE DVI_ENABLE_IRQ_STATS=1)
endif()

add_executable(dvi_hdmi_example example/dvi_hdmi_example.cpp)
target_link_libraries(dvi_hdmi_example PRIVATE
    pico_toolset_dvi_hdmi
    pico_stdlib
    pico_multicore
    pico_sync
)
pico_add_extra_outputs(dvi_hdmi_example)


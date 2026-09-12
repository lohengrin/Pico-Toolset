/*
 * HDMI data-island packet types and encoders: CEA-861 InfoFrames (AVI,
 * audio), the audio-clock-regeneration (ACR) packet, and the audio-sample
 * packets that ride in a scanline's horizontal blanking interval, TERC4-
 * encoded (with BCH parity) the way an HDMI receiver expects.
 *
 * New file, not a vendored one -- part of TOM6809's HDMI-audio addition
 * (see README.md and CMakeLists.txt's "PATCH
 * (hdmi-audio):" notes). The TERC4/BCH-parity encode algorithm and the
 * InfoFrame/ACR/audio-sample packet layouts are ported from Shuichi
 * Takano's dvi::DataPacket (github.com/shuichitakano/pico_lib, dvi/
 * data_packet.{h,cpp}, MIT License, Copyright (c) 2021 Shuichi Takano),
 * translated from its original C++ class into the C-linkage free-function
 * shape rh1tech/frank-hdmi-audio's frank_data_packet.{c,h} already uses
 * (BSD-3-Clause, layered on Wren6991/PicoDVI) so it fits this project's
 * plain-C dvi.c/dvi.h the same way frank's does.
 *
 * Plain C (see audio_ring.h's identical rationale) -- dvi.c embeds
 * data_packet_t members directly in struct dvi_inst and is compiled as C.
 */
#ifndef TOM6809_DATA_PACKET_H
#define TOM6809_DATA_PACKET_H

#include "audio_ring.h"
#include <stdbool.h>
#include <stdint.h>

// Self-contained rather than pulling these from dvi_config_defs.h: that
// header unconditionally includes "hardware/platform_defs.h"/"pico/config.h"
// (Pico-SDK-only), which would make this header (and anything that includes
// it, including this project's native ctest build) un-host-buildable. Same
// approach frank_data_packet.h already takes for the same reason.
#define TOM6809_TMDS_CHANNELS 3
#define TOM6809_W_GUARDBAND 2
#define TOM6809_W_PREAMBLE 8
#define TOM6809_W_DATA_PACKET 32
#ifndef DVI_SYMBOLS_PER_WORD
#define DVI_SYMBOLS_PER_WORD 2
#endif
#define TOM6809_W_DATA_ISLAND (TOM6809_W_GUARDBAND * 2 + TOM6809_W_DATA_PACKET)
#define N_DATA_ISLAND_WORDS (TOM6809_W_DATA_ISLAND / DVI_SYMBOLS_PER_WORD)

#if defined(PICO_ON_DEVICE)
#include "pico.h"
#define TOM6809_DATA_PACKET_FUNC(name) __not_in_flash_func(name)
// Lookup tables read from the per-scanline core1 DMA IRQ must live in RAM,
// not flash .rodata -- see this header's "RAM residency" note below.
#define TOM6809_DATA_PACKET_RAM_DATA __not_in_flash("tom6809_hdmi_audio")
#else
#define TOM6809_DATA_PACKET_FUNC(name) name
#define TOM6809_DATA_PACKET_RAM_DATA
#endif

/*
 * RAM residency (why TOM6809_DATA_PACKET_FUNC/_RAM_DATA are not optional)
 * ----------------------------------------------------------------------
 * Everything reachable from dvi_dma_irq_handler() (dvi.c) runs once per
 * scanline on core1 and must finish inside the horizontal active region --
 * 640 pixels = 6400 clk_sys cycles at this project's 252MHz/25.2MHz pixel
 * clock. Upstream libdvi is scrupulous about this: every function on that
 * path is __not_in_flash_func/__scratch_x and it touches *zero* flash.
 *
 * That is not mere tidiness. On RP2350 the XIP flash and the PSRAM share
 * one QMI controller, and this project's core0 hammers both (6809
 * interpretation out of flash, machine RAM out of PSRAM). A flash access
 * from the core1 IRQ therefore does not merely risk an XIP cache miss, it
 * risks queueing behind core0's in-flight QMI traffic. Miss the active
 * region deadline and the DMA control chain is not reloaded in time, which
 * loses the picture completely -- not a glitch, no crash, core0 (serial,
 * stats) entirely unaffected.
 *
 * Confirmed on real hardware exactly that way: an earlier revision of this
 * patch left data_packet_set_null(), dvi_update_data_packet_(),
 * audio_ring_advance_read() and *both* tables below in flash. Video was
 * fine through the boot menu (core0 idle, so QMI was quiet and the XIP
 * cache stayed warm) and died the instant an emulated machine started
 * (core0 saturating QMI). Four successive redesigns of the data-island DMA
 * *structure* changed nothing, because the DMA structure was never the
 * problem.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SCAN_INFO_NO_DATA, SCAN_INFO_OVERSCAN, SCAN_INFO_UNDERSCAN } scan_info_t;

typedef enum { PIXEL_FORMAT_RGB, PIXEL_FORMAT_YCBCR422, PIXEL_FORMAT_YCBCR444 } pixel_format_t;

typedef enum {
    COLORIMETRY_NO_DATA,
    COLORIMETRY_ITU601,
    COLORIMETRY_ITU709,
    COLORIMETRY_EXTENDED
} colorimetry_t;

typedef enum {
    PICTURE_ASPECT_RATIO_NO_DATA,
    PICTURE_ASPECT_RATIO_4_3,
    PICTURE_ASPECT_RATIO_16_9
} picture_aspect_ratio_t;

typedef enum {
    ACTIVE_FORMAT_ASPECT_RATIO_NO_DATA = -1,
    ACTIVE_FORMAT_ASPECT_RATIO_SAME_AS_PAR = 8,
    ACTIVE_FORMAT_ASPECT_RATIO_4_3,
    ACTIVE_FORMAT_ASPECT_RATIO_16_9,
    ACTIVE_FORMAT_ASPECT_RATIO_14_9
} active_format_aspect_ratio_t;

typedef enum { RGB_QUANTIZATION_DEFAULT, RGB_QUANTIZATION_LIMITED, RGB_QUANTIZATION_FULL } rgb_quantization_range_t;

typedef enum {
    VIDEO_CODE_640x480P60 = 1,
    VIDEO_CODE_720x480P60 = 2,
    VIDEO_CODE_1280x720P60 = 4,
    VIDEO_CODE_1920x1080I60 = 5,
    VIDEO_CODE_720x576P50 = 17
} video_code_t;

typedef struct data_packet {
    uint8_t header[4];
    uint8_t subpacket[4][8];
} data_packet_t;

typedef struct data_island_stream {
    uint32_t data[TOM6809_TMDS_CHANNELS][N_DATA_ISLAND_WORDS];
} data_island_stream_t;

void data_packet_compute_parity(data_packet_t *packet);

// RAM-resident: called every scanline from the core1 DMA IRQ whenever
// dvi_update_data_packet_() declines to supply a packet (see the RAM
// residency note above).
void TOM6809_DATA_PACKET_FUNC(data_packet_set_null)(data_packet_t *packet);

// Idle data-island content: what dvi_timing.c's *_with_audio() scanline
// builders point the DMA at when they first build each list, before
// dvi_audio_init()'s dvi_update_data_island_ptr() call (dvi.c) repoints
// every list at the real, per-scanline-encoded next_data_stream instead.
// `_12` is lanes 1/2's shared idle subpacket content; `_0` is lane 0's own
// (vsync/hsync-state-dependent) idle header content. Both return a pointer
// to N_DATA_ISLAND_WORDS words of static storage; callers don't own it.
const uint32_t *data_packet_get_default_island_12(void);
const uint32_t *data_packet_get_default_island_0(bool vsync, bool hsync);

// AVI InfoFrame (CEA-861 Type 0x82) -- static per-mode video-format
// metadata (aspect ratio, colorimetry, scan/quantization). Sent once every
// other frame in dvi_dma_irq_handler()'s front-porch slot, alternating with
// the audio InfoFrame below.
void data_packet_set_avi_info_frame(data_packet_t *packet, scan_info_t scan, pixel_format_t pixel_format,
                                     colorimetry_t colorimetry, picture_aspect_ratio_t picture_aspect_ratio,
                                     active_format_aspect_ratio_t active_format_aspect_ratio,
                                     rgb_quantization_range_t rgb_quantization_range, video_code_t video_code);

// Audio InfoFrame (CEA-861 Type 0x84) -- channel count/coding type/sample
// size/sample frequency. `freq` is the nominal audio sample rate in Hz
// (32000/44100/48000 map to specific CEA-861 SF codes; anything else is
// reported as "refer to stream header").
void data_packet_set_audio_info_frame(data_packet_t *packet, int freq);

// Audio Clock Regeneration packet (CEA-861 Type 0x01) -- tells the receiver
// how to regenerate the audio sample clock from the TMDS clock:
// 128 * audio_freq = pixel_clock * N / CTS. Pick N from the CEA-861 table
// for the chosen sample rate and derive CTS from the actual pixel clock
// (dvi_set_audio_freq() in dvi.h does this arithmetic).
void data_packet_set_audio_clock_regeneration(data_packet_t *packet, int cts, int n);

// Audio Sample packet (CEA-861 Type 0x02) -- pulls up to 4 stereo frames
// straight out of `ring` (advancing its read pointer by however many frames
// it actually consumed) and returns the updated running sample-frame count
// (0-191, CEA-861's "B" bit / frame-count-of-192 convention for the IEC
// 60958 channel-status block start marker) for the next call's `frame_ct`.
int TOM6809_DATA_PACKET_FUNC(data_packet_set_audio_sample)(data_packet_t *packet, audio_ring_t *ring, int n,
                                                            int frame_ct);

// TERC4-encodes `packet` (with BCH parity) into `stream`, ready for the DMA
// lists built by dvi_setup_scanline_for_*_with_audio() to feed straight to
// the TMDS serialiser -- see data_packet.cpp's doc comment for the wire
// format. `vsync`/`hsync` are the *polarity-corrected* current sync state
// (matching dvi_timing_state's v_state/the timing's own sync polarity),
// carried in every data-island guardband/header symbol per the HDMI spec.
void TOM6809_DATA_PACKET_FUNC(data_packet_encode)(data_island_stream_t *stream, const data_packet_t *packet,
                                                   bool vsync, bool hsync);

#ifdef __cplusplus
}
#endif

#endif /* TOM6809_DATA_PACKET_H */

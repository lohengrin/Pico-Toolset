#pragma once

// Known-good I2sAudioConfig presets for specific boards.

#include "pico_toolset/i2s_audio.h"

namespace pico_toolset::configs::i2s_audio {

// The "Pico DV" carrier board's onboard PCM5100A-style I2S DAC
// (DATA=GPIO26, BCK=GPIO27, LRCK=GPIO28). DMA channel 0 and PIO SM 0 are
// free on this board's own component mix (no other driver claims a fixed
// DMA channel by hardcoded number). Validated on real hardware.
inline constexpr I2sAudioConfig kPicoDvCarrier = {
    .sample_rate_hz = 44100,
    .channel_count = 1,
    .pin_data = 26,
    .pin_clock_base = 27,
    .dma_channel = 0,
    .pio_sm = 0,
    .buffer_count = 4,
    .samples_per_buffer = 882,
};

} // namespace pico_toolset::configs::i2s_audio

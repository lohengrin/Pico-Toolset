#pragma once

#include <cstdint>
#include <span>

// pico-extras' pico_audio_i2s type, forward-declared at global scope (where
// it's actually defined -- pico-extras has no namespace) so I2sAudioOutput
// below can hold a pointer to it without pulling in pico/audio.h here.
struct audio_buffer_pool;

namespace pico_toolset {

// Configuration for an I2S DAC output (e.g. a PCM5100A) via pico-extras'
// pico_audio_i2s. Every pin, clock, and buffer parameter is configurable --
// nothing is hardcoded in the driver. BCK/LRCK must be consecutive GPIOs
// (LRCK = pin_clock_base + 1); this is pico_audio_i2s's own hardware
// constraint, not one this driver adds.
//
// pin_data/pin_clock_base/dma_channel have no working default -- they're
// board wiring and DMA-channel budget, not driver behavior -- see
// i2s_audio_configs.h for known-good per-board presets (e.g.
// configs::i2s_audio::kPicoDvCarrier), or set them yourself for a board
// without one yet.
struct I2sAudioConfig {
    uint32_t sample_rate_hz = 44100;
    uint8_t  channel_count  = 1;    // 1 = mono in (pico_audio_i2s replicates to stereo L/R automatically), 2 = stereo in
    uint8_t  pin_data;               // I2S DATA -- must be set
    uint8_t  pin_clock_base;         // I2S BCK; LRCK is this pin + 1 -- must be set

    // pico_audio_i2s's own audio_i2s_setup() calls dma_channel_claim() on
    // this EXACT channel number (not an "any free channel" search like
    // pico_toolset::Ili9486Config::dma_channel) -- pick one none of your
    // other drivers also claim, or audio_i2s_setup() panics with "DMA
    // channel N is already claimed" if it's taken.
    uint8_t  dma_channel;            // must be set -- see above
    uint8_t  pio_sm         = 0;    // PIO state machine index within pico_audio_i2s's PIO block

    int      buffer_count        = 4;   // producer pool depth -- slack so a dropped/late frame doesn't stutter
    int      samples_per_buffer  = 882; // default: 20ms at 44.1kHz (sample_rate_hz * 0.02)
};

// Float-sample ([-1,1]) I2S audio output, converting to 16-bit PCM
// internally. Requires pico-extras' pico_audio_i2s target to already exist
// in the consuming build (see this component's CMakeLists.txt and the
// toolset README's "I2S audio (pico-extras)" section) -- pico_extras_import
// must run before project(), same constraint as pico_sdk_import, so this
// component cannot own that setup itself the way e.g. the USB HID
// component owns fetching Pico-PIO-USB.
class I2sAudioOutput {
public:
    // Sets up pico_audio_i2s's producer pool and PIO/DMA-backed output.
    // Returns false if the producer pool couldn't be allocated.
    bool init(const I2sAudioConfig& config);

    // Queues float samples for playback. Non-blocking: if no producer
    // buffer is free right now, this call's audio is dropped and the
    // function returns immediately -- dropping a frame beats blocking a
    // real-time caller waiting for a DMA buffer. Samples beyond what fits
    // are ignored; if fewer are given than the buffer holds, the remainder
    // is filled with silence.
    void queue_samples(std::span<const float> samples);

private:
    audio_buffer_pool* m_producer_pool = nullptr;
    uint32_t m_samples_per_buffer = 882;
};

} // namespace pico_toolset

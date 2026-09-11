#include "pico_toolset/i2s_audio.h"

#include "pico/audio_i2s.h"

#include <algorithm>
#include <cstdint>

namespace pico_toolset {

bool I2sAudioOutput::init(const I2sAudioConfig& config) {
    m_samples_per_buffer = static_cast<uint32_t>(config.samples_per_buffer);

    static audio_format_t audio_format{
        .sample_freq = config.sample_rate_hz,
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = config.channel_count,
    };
    static audio_buffer_format_t producer_format{
        .format = &audio_format,
        .sample_stride = 2, // sizeof(int16_t)
    };

    m_producer_pool = audio_new_producer_pool(&producer_format, config.buffer_count, config.samples_per_buffer);
    if (!m_producer_pool) {
        return false;
    }

    audio_i2s_config_t i2s_config{
        .data_pin = config.pin_data,
        .clock_pin_base = config.pin_clock_base,
        .dma_channel = config.dma_channel,
        .pio_sm = config.pio_sm,
    };
    audio_i2s_setup(&audio_format, &i2s_config);
    audio_i2s_connect(m_producer_pool);
    audio_i2s_set_enabled(true);
    return true;
}

void I2sAudioOutput::queue_samples(std::span<const float> samples) {
    // Non-blocking: dropping a frame's audio under load beats blocking a
    // real-time caller waiting for a DMA buffer.
    audio_buffer_t* buffer = take_audio_buffer(m_producer_pool, false);
    if (!buffer) return;

    auto* out = reinterpret_cast<int16_t*>(buffer->buffer->bytes);
    uint32_t count = std::min(static_cast<uint32_t>(samples.size()), buffer->max_sample_count);
    for (uint32_t i = 0; i < count; ++i) {
        float clamped = std::clamp(samples[i], -1.0f, 1.0f);
        out[i] = static_cast<int16_t>(clamped * 32767.0f);
    }
    for (uint32_t i = count; i < buffer->max_sample_count; ++i) {
        out[i] = 0;
    }
    buffer->sample_count = buffer->max_sample_count;
    give_audio_buffer(m_producer_pool, buffer);
}

} // namespace pico_toolset

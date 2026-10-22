#pragma once

#include <cstddef>
#include <cstdint>

namespace pico_toolset {

// A producer of interleaved signed 16-bit PCM. The consumer (e.g. bluetooth_a2dp) calls
// prepare() each time playback (re)starts, because the negotiated sample rate and channel
// count can differ between connections, then fill() repeatedly from its own context -- keep
// fill() short and free of blocking calls.
class AudioSource {
public:
    virtual ~AudioSource() = default;

    virtual void prepare(uint32_t sample_rate_hz, uint8_t channels) {
        m_rate = sample_rate_hz;
        m_channels = channels;
    }

    // Writes `frames` frames (frames * channels samples) of interleaved PCM.
    virtual void fill(int16_t* pcm, size_t frames) = 0;

protected:
    uint32_t m_rate = 44100;
    uint8_t m_channels = 2;
};

} // namespace pico_toolset

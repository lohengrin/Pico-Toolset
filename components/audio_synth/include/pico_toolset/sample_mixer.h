#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pico_toolset {

// Polyphonic player for 8-bit unsigned PCM samples (the format of DOOM's DMX lumps, Sound Blaster
// .wav bodies, many retro sample banks): N voices, each with its own source rate, volume and
// playback speed, mixed with linear interpolation into a caller-supplied 32-bit accumulator. Pure
// arithmetic, no hardware and no allocation.
//
// Not thread-safe by itself: guard start()/stop()/set_volume()/stop_all() against a concurrent
// mix() with whatever fits the application (an interrupt-off section, a mutex, or calling both
// from one context). The samples must stay valid, and readable from the mixing context, for as
// long as a voice plays them (SRAM or flash; not PSRAM if the mixer runs where XIP is unsafe).
template <int kVoices = 8>
class SampleMixer {
public:
    // Output rate in Hz. Changing it stops every voice (their steps depend on it).
    void set_rate(uint32_t hz) {
        if (hz == m_rate) return;
        m_rate = hz;
        stop_all();
    }
    [[nodiscard]] uint32_t rate() const { return m_rate; }

    // Starts `handle` on a free voice, else steals the voice with the lowest handle (handles
    // should grow with time, so that is the oldest). `volume` 0..127 scales the voice by
    // volume (full scale = +-127 * 127 in the accumulator); `speed` 1.0 plays at the source's
    // pitch, 2.0 an octave up. False for an empty sample.
    bool start(int handle, const uint8_t* data, uint32_t length, uint32_t source_rate_hz, int volume,
               float speed = 1.0f) {
        if (length < 2 || source_rate_hz == 0 || m_rate == 0) return false;
        Voice* slot = nullptr;
        for (Voice& v : m_voices)
            if (!v.active) { slot = &v; break; }
        if (!slot) {
            slot = &m_voices[0];
            for (Voice& v : m_voices)
                if (v.handle < slot->handle) slot = &v;
        }
        const double step = static_cast<double>(source_rate_hz) * 65536.0 / static_cast<double>(m_rate) * speed;
        slot->data = data;
        slot->length = length;
        slot->pos = 0;
        slot->step = std::max<uint32_t>(1, static_cast<uint32_t>(step));
        slot->volume = std::clamp(volume, 0, 127);
        slot->handle = handle;
        slot->active = true;
        return true;
    }

    void stop(int handle) {
        for (Voice& v : m_voices)
            if (v.active && v.handle == handle) v.active = false;
    }
    void stop_all() {
        for (Voice& v : m_voices) v.active = false;
    }
    void set_volume(int handle, int volume) {
        for (Voice& v : m_voices)
            if (v.active && v.handle == handle) v.volume = std::clamp(volume, 0, 127);
    }
    [[nodiscard]] bool playing(int handle) const {
        for (const Voice& v : m_voices)
            if (v.active && v.handle == handle) return true;
        return false;
    }
    [[nodiscard]] bool active() const {
        for (const Voice& v : m_voices)
            if (v.active) return true;
        return false;
    }

    // Adds `count` mono samples of every active voice into acc[]. Finished voices free themselves.
    void mix(int32_t* acc, int count) {
        for (Voice& v : m_voices) {
            if (!v.active) continue;
            uint32_t pos = v.pos;
            const uint32_t step = v.step, len = v.length;
            const uint8_t* data = v.data;
            const int vol = v.volume;
            for (int i = 0; i < count; ++i) {
                const uint32_t idx = pos >> 16;
                if (idx + 1 >= len) { v.active = false; break; }
                const int s0 = data[idx], s1 = data[idx + 1];
                // Interpolate in 8.8 fixed point and scale once: no 8-bit truncation of the blend.
                const int s = (s0 - 128) * 256 + (s1 - s0) * static_cast<int>((pos >> 8) & 0xFF);
                acc[i] += (s * vol) >> 8;
                pos += step;
            }
            v.pos = pos;
        }
    }

private:
    struct Voice {
        bool active = false;
        int handle = 0;
        const uint8_t* data = nullptr;
        uint32_t length = 0;
        uint32_t pos = 0;  // 16.16 source position
        uint32_t step = 0; // 16.16 source samples per output sample
        int volume = 0;
    };
    Voice m_voices[kVoices];
    uint32_t m_rate = 44100;
};

} // namespace pico_toolset

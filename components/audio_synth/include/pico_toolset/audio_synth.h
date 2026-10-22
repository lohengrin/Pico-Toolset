#pragma once

#include <cmath>
#include <cstdint>

#include "pico_toolset/audio_source.h"

namespace pico_toolset {

enum class Waveform : uint8_t { Sine, Square, Triangle, Sawtooth };

namespace detail {

// 256-entry quarter-resolution sine table with one guard entry, built at compile time
// (Taylor series after range reduction, so no runtime init and no libm call).
struct SineTable {
    int16_t v[257];
    constexpr SineTable() : v() {
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i <= 256; ++i) {
            double x = 2.0 * pi * i / 256.0; // 0..2pi
            if (x > pi) x -= 2.0 * pi;       // -pi..pi
            double term = x, sum = x;
            for (int n = 1; n < 12; ++n) {
                term *= -x * x / ((2.0 * n) * (2.0 * n + 1.0));
                sum += term;
            }
            v[i] = static_cast<int16_t>(sum * 32767.0 + (sum >= 0 ? 0.5 : -0.5));
        }
    }
};
inline constexpr SineTable kSineTable{};

// One waveform sample (signed 16-bit full scale) for a 32-bit phase.
inline int16_t wave_sample(Waveform w, uint32_t phase) {
    switch (w) {
    case Waveform::Square:   return phase & 0x80000000u ? -32767 : 32767;
    case Waveform::Sawtooth: return static_cast<int16_t>((phase >> 16) - 32768);
    case Waveform::Triangle: {
        const int32_t p = static_cast<int32_t>(phase >> 16); // 0..65535
        const int32_t t = p < 32768 ? p : 65535 - p;         // 0..32767..0
        return static_cast<int16_t>(t * 2 - 32767);
    }
    default: { // Sine, linear interpolation between table entries
        const uint32_t idx = phase >> 24;
        const int32_t frac = static_cast<int32_t>((phase >> 8) & 0xFFFF);
        const int32_t a = kSineTable.v[idx], b = kSineTable.v[idx + 1];
        return static_cast<int16_t>(a + (((b - a) * frac) >> 16));
    }
    }
}

} // namespace detail

// Continuous tone. amplitude 0..1 of full scale (default 0.5 -- leave headroom).
class ToneSource : public AudioSource {
public:
    explicit ToneSource(Waveform waveform = Waveform::Sine, float frequency_hz = 440.0f, float amplitude = 0.5f)
        : m_wave(waveform), m_freq(frequency_hz) {
        set_amplitude(amplitude);
    }

    void set_waveform(Waveform w) { m_wave = w; }
    void set_frequency(float hz) { m_freq = hz; update_step(); }
    void set_amplitude(float a) {
        m_amp = static_cast<int32_t>((a < 0 ? 0 : a > 1 ? 1 : a) * 32767.0f);
    }
    [[nodiscard]] float frequency() const { return m_freq; }

    void prepare(uint32_t sample_rate_hz, uint8_t channels) override {
        AudioSource::prepare(sample_rate_hz, channels);
        update_step();
    }

    void fill(int16_t* pcm, size_t frames) override {
        for (size_t i = 0; i < frames; ++i) {
            const int16_t s = static_cast<int16_t>((detail::wave_sample(m_wave, m_phase) * m_amp) >> 15);
            m_phase += m_step;
            for (uint8_t c = 0; c < m_channels; ++c) *pcm++ = s;
        }
    }

private:
    void update_step() { m_step = static_cast<uint32_t>(static_cast<double>(m_freq) * 4294967296.0 / m_rate); }

    Waveform m_wave;
    float m_freq;
    int32_t m_amp = 16383;
    uint32_t m_phase = 0;
    uint32_t m_step = 0;
};

// Exponential frequency sweep f0 -> f1 over `seconds`, repeating.
class SweepSource : public AudioSource {
public:
    SweepSource(float f0_hz = 200.0f, float f1_hz = 2000.0f, float seconds = 4.0f, float amplitude = 0.5f)
        : m_f0(f0_hz), m_f1(f1_hz), m_seconds(seconds), m_tone(Waveform::Sine, f0_hz, amplitude) {}

    void prepare(uint32_t sample_rate_hz, uint8_t channels) override {
        AudioSource::prepare(sample_rate_hz, channels);
        m_tone.prepare(sample_rate_hz, channels);
        m_pos = 0;
    }

    void fill(int16_t* pcm, size_t frames) override {
        const uint32_t total = static_cast<uint32_t>(m_seconds * m_rate);
        size_t done = 0;
        while (done < frames) {
            const size_t chunk = frames - done < 256 ? frames - done : 256; // retune every 256 frames
            const float t = total ? static_cast<float>(m_pos) / static_cast<float>(total) : 0.0f;
            m_tone.set_frequency(m_f0 * std::pow(m_f1 / m_f0, t));
            m_tone.fill(pcm + done * m_channels, chunk);
            done += chunk;
            m_pos += static_cast<uint32_t>(chunk);
            if (total && m_pos >= total) m_pos = 0;
        }
    }

private:
    float m_f0, m_f1, m_seconds;
    ToneSource m_tone;
    uint32_t m_pos = 0;
};

// One note of a melody; freq_hz == 0 is a rest.
struct AudioNote {
    uint16_t freq_hz;
    uint16_t duration_ms;
};

// Plays a note list (optionally looping) with a short fade in/out per note to avoid clicks.
class MelodySource : public AudioSource {
public:
    MelodySource(const AudioNote* notes, size_t count, bool loop = true, Waveform waveform = Waveform::Sine,
                 float amplitude = 0.5f)
        : m_notes(notes), m_count(count), m_loop(loop), m_tone(waveform, 440.0f, amplitude) {}

    void prepare(uint32_t sample_rate_hz, uint8_t channels) override {
        AudioSource::prepare(sample_rate_hz, channels);
        m_tone.prepare(sample_rate_hz, channels);
        m_index = 0;
        start_note();
    }

    void fill(int16_t* pcm, size_t frames) override {
        for (size_t i = 0; i < frames; ++i) {
            int16_t s = 0;
            if (m_active) {
                int16_t tmp[2];
                m_tone.fill(tmp, 1);
                int32_t gain = 32767;
                const uint32_t ramp = m_rate / 200; // 5 ms
                if (m_pos < ramp) gain = static_cast<int32_t>(32767 * m_pos / ramp);
                else if (m_left < ramp) gain = static_cast<int32_t>(32767 * m_left / ramp);
                s = static_cast<int16_t>((tmp[0] * gain) >> 15);
            }
            for (uint8_t c = 0; c < m_channels; ++c) *pcm++ = s;
            ++m_pos;
            if (m_left && --m_left == 0) next_note();
        }
    }

private:
    void start_note() {
        m_active = false;
        m_pos = 0;
        m_left = 0;
        if (m_count == 0) return;
        const AudioNote& n = m_notes[m_index];
        m_left = static_cast<uint32_t>(n.duration_ms) * m_rate / 1000u;
        if (m_left == 0) m_left = 1;
        if (n.freq_hz) {
            m_tone.set_frequency(n.freq_hz);
            m_active = true;
        }
    }
    void next_note() {
        ++m_index;
        if (m_index >= m_count) {
            if (!m_loop) { m_count = 0; m_active = false; return; }
            m_index = 0;
        }
        start_note();
    }

    const AudioNote* m_notes;
    size_t m_count;
    bool m_loop;
    ToneSource m_tone;
    size_t m_index = 0;
    uint32_t m_pos = 0, m_left = 0;
    bool m_active = false;
};

// Digital silence (keeps a stream alive without sound).
class SilenceSource : public AudioSource {
public:
    void fill(int16_t* pcm, size_t frames) override {
        for (size_t i = 0; i < frames * m_channels; ++i) pcm[i] = 0;
    }
};

} // namespace pico_toolset

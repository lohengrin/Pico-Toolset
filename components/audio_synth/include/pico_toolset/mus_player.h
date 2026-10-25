#pragma once

#include <cstddef>
#include <cstdint>

#include "pico_toolset/audio_source.h"

namespace pico_toolset {

// Player for id Software's MUS music format (DOOM / Heretic / Hexen scores): parses the score and
// plays it on a small chip-tune synthesiser -- 10 voices of square / pulse / triangle / saw / noise
// shaped by General-MIDI program groups, with GM percussion approximated by noise and a pitched
// kick/tom. It is a character-rich stand-in for the original OPL/GM sound, not an emulation.
// Pure arithmetic: no hardware, no allocation, no libc beyond <cmath>.
//
// Timing: the score runs at 140 ticks per second and advances by the number of samples rendered, so
// render() (or step()) must be driven at the real output rate. The tick length is `rate / 140`
// samples (an integer; the sub-sample error is below 0.3 % at the usual 44.1 / 48 kHz).
//
// Not thread-safe: start/stop/pause/resume/set_volume/render/step/lead must not run concurrently
// (guard with an interrupt-off section or call from one context). The score data must stay valid
// and readable from the rendering context while it plays.
class MusPlayer {
public:
    explicit MusPlayer(uint32_t sample_rate_hz = 44100);

    // `mus_lump` starts with "MUS\x1a". Replaces any song playing. False if it is not a MUS lump.
    bool start(const void* mus_lump, bool looping);
    void stop();
    void pause();   // notes are released; the score position is kept
    void resume();
    // 0..127. Scales the mix; 0 renders silence (the sequencer still runs).
    void set_volume(int volume);

    // Playing and not paused.
    [[nodiscard]] bool playing() const { return m_playing && !m_paused; }

    // Adds the next `n` samples of the song into both buffers (mono signal, identical in l and r),
    // advancing the score. Does nothing when not playing and no note is still ringing.
    void render(float* mix_l, float* mix_r, int n);

    // For tone-only outputs (a single-voice piezo): advances the score by `ticks` ticks (1/140 s)
    // WITHOUT synthesising, ageing the note envelopes in tick units, then lead() picks a melody note.
    void step(int ticks);
    // Pitch (Hz) and loudness (0..1) of the loudest, highest melodic voice; false if silent.
    bool lead(float* hz, float* level) const;

private:
    enum Shape : uint8_t { Square, Pulse, Tri, Saw, Noise };

    static constexpr int kTickHz = 140;
    static constexpr int kVoices = 10;
    static constexpr int kChannels = 16;
    static constexpr int kPercussionChannel = 15;

    struct Channel {
        uint8_t program = 0;
        uint8_t volume = 100;
        uint8_t expression = 127;
        uint8_t last_vel = 100;
        uint8_t bend = 128; // 0..255, 128 centred
    };
    struct Voice {
        bool active = false;
        bool released = false;
        uint8_t chan = 0;
        uint8_t note = 0;
        uint8_t vel = 0;
        Shape shape = Square;
        uint32_t phase = 0;
        uint32_t inc = 0;
        uint32_t rng = 1;
        float env = 0.0f;
        float decay = 1.0f; // per-sample envelope multiplier
        uint32_t serial = 0;
    };

    float note_freq(int note, int bend) const;
    uint32_t freq_to_inc(float hz) const;
    float scaled_decay(float per_sample_at_44k1) const;
    void program_voice(int program, Shape& shape, float& decay) const;
    void percussion_voice(int note, Shape& shape, uint32_t& inc, float& decay) const;
    void start_note(int chan, int note, int vel);
    void release_voice(Voice& v);
    void note_off(int chan, int note);
    void channel_notes_off(int chan);
    void all_notes_off();
    void reset_channels();
    void set_bend(int chan, int bend);
    bool at_end() const { return m_pos >= m_end; }
    void run_events();
    template <Shape S>
    void render_voice(Voice& v, float* out_l, float* out_r, int n, float gain);
    void render_voices(float* out_l, float* out_r, int n);
    bool any_voice_active() const;

    uint32_t m_rate;
    int m_samples_per_tick;
    float m_release_decay;

    Channel m_chan[kChannels];
    Voice m_voice[kVoices];
    uint32_t m_serial = 0;

    const uint8_t* m_score = nullptr;
    const uint8_t* m_pos = nullptr;
    const uint8_t* m_end = nullptr;
    bool m_playing = false;
    bool m_paused = false;
    bool m_looping = false;
    int m_delay_ticks = 0;
    int m_samples_to_tick;
    float m_master = 0.0f;
};

// The player as an AudioSource (mono, copied to every output channel), for sinks such as the
// A2DP source. `volume` as MusPlayer::set_volume(); the player must outlive the source.
class MusSource final : public AudioSource {
public:
    explicit MusSource(MusPlayer& player) : m_player(player) {}
    void fill(int16_t* pcm, size_t frames) override;

private:
    MusPlayer& m_player;
};

} // namespace pico_toolset

#include "pico_toolset/mus_player.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pico_toolset {

namespace {

// Per-voice peak before the master scale below; 10 voices at full velocity would sum to 1.8, so
// master is kept well under 1 -- typical scores keep a handful of voices sounding at once.
constexpr float kVoiceGain = 0.18f;
constexpr float kMasterScale = 0.5f;
constexpr float kReleaseDecay44k1 = 0.99887f; // ~20 ms time constant at 44.1 kHz
constexpr float kSilence = 0.002f;

uint16_t rd16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

} // namespace

MusPlayer::MusPlayer(uint32_t sample_rate_hz)
    : m_rate(sample_rate_hz ? sample_rate_hz : 44100),
      m_samples_per_tick(static_cast<int>((sample_rate_hz ? sample_rate_hz : 44100) / kTickHz)),
      m_samples_to_tick(m_samples_per_tick) {
    m_release_decay = scaled_decay(kReleaseDecay44k1);
}

float MusPlayer::note_freq(int note, int bend) const {
    const float semis = static_cast<float>(note - 69) + static_cast<float>(bend - 128) * (2.0f / 128.0f);
    return 440.0f * std::exp2(semis * (1.0f / 12.0f));
}

uint32_t MusPlayer::freq_to_inc(float hz) const {
    return static_cast<uint32_t>(hz * (4294967296.0f / static_cast<float>(m_rate)));
}

// The envelope constants below were tuned per sample at 44.1 kHz; keep the same time constant at
// other rates.
float MusPlayer::scaled_decay(float per_sample_at_44k1) const {
    if (m_rate == 44100 || per_sample_at_44k1 >= 1.0f) return per_sample_at_44k1;
    return std::pow(per_sample_at_44k1, 44100.0f / static_cast<float>(m_rate));
}

// Shape/decay per GM program group (melodic channels).
void MusPlayer::program_voice(int program, Shape& shape, float& decay) const {
    decay = 1.0f; // sustained
    if (program < 8) { shape = Pulse; decay = scaled_decay(0.99985f); }          // pianos
    else if (program < 16) { shape = Tri; decay = scaled_decay(0.9997f); }       // chromatic percussion
    else if (program < 24) { shape = Square; }                                   // organs
    else if (program < 32) { shape = Saw; decay = scaled_decay(0.9999f); }       // guitars
    else if (program < 40) { shape = Tri; }                                      // basses
    else if (program < 48) { shape = Tri; }                                      // strings
    else if (program < 56) { shape = Saw; }                                      // ensemble/voices
    else if (program < 72) { shape = Square; }                                   // brass/reeds
    else if (program < 80) { shape = Tri; }                                      // pipes
    else if (program < 88) { shape = Square; }                                   // synth leads
    else if (program < 104) { shape = Tri; }                                     // pads/fx
    else { shape = Pulse; decay = scaled_decay(0.9998f); }                       // ethnic/percussive
}

// Percussion: GM drum note -> voice shape, rate and decay.
void MusPlayer::percussion_voice(int note, Shape& shape, uint32_t& inc, float& decay) const {
    switch (note) {
    case 35: case 36: // kick
        shape = Tri; inc = freq_to_inc(55.0f); decay = scaled_decay(0.9996f); break;
    case 38: case 40: // snare
        shape = Noise; inc = freq_to_inc(9000.0f); decay = scaled_decay(0.9996f); break;
    case 41: case 43: case 45: case 47: case 48: case 50: // toms
        shape = Tri; inc = freq_to_inc(note_freq(note + 12, 128)); decay = scaled_decay(0.9995f); break;
    case 42: case 44: // closed hats
        shape = Noise; inc = freq_to_inc(22000.0f); decay = scaled_decay(0.9990f); break;
    case 46: case 49: case 51: case 52: case 53: case 55: case 57: case 59: // open hat / cymbals
        shape = Noise; inc = freq_to_inc(22000.0f); decay = scaled_decay(0.9999f); break;
    default:
        shape = Noise; inc = freq_to_inc(14000.0f); decay = scaled_decay(0.9993f); break;
    }
}

void MusPlayer::start_note(int chan, int note, int vel) {
    Voice* slot = nullptr;
    for (Voice& v : m_voice) {
        if (v.active && v.chan == chan && v.note == note) { // retrigger
            slot = &v;
            break;
        }
    }
    if (!slot) {
        for (Voice& v : m_voice) {
            if (!v.active) { slot = &v; break; }
        }
    }
    if (!slot) { // steal the quietest voice
        slot = &m_voice[0];
        for (Voice& v : m_voice) {
            if (v.env < slot->env)
                slot = &v;
        }
    }

    Voice& v = *slot;
    v.active = true;
    v.released = false;
    v.chan = static_cast<uint8_t>(chan);
    v.note = static_cast<uint8_t>(note);
    v.vel = static_cast<uint8_t>(vel);
    v.phase = 0;
    v.env = 1.0f;
    v.serial = ++m_serial;
    if (chan == kPercussionChannel) {
        percussion_voice(note, v.shape, v.inc, v.decay);
    } else {
        program_voice(m_chan[chan].program, v.shape, v.decay);
        v.inc = freq_to_inc(note_freq(note, m_chan[chan].bend));
    }
}

void MusPlayer::release_voice(Voice& v) {
    v.released = true;
    v.decay = m_release_decay;
}

void MusPlayer::note_off(int chan, int note) {
    if (chan == kPercussionChannel)
        return; // percussion rings out on its own decay
    for (Voice& v : m_voice) {
        if (v.active && !v.released && v.chan == chan && v.note == note)
            release_voice(v);
    }
}

void MusPlayer::channel_notes_off(int chan) {
    for (Voice& v : m_voice) {
        if (v.active && !v.released && v.chan == chan)
            release_voice(v);
    }
}

void MusPlayer::all_notes_off() {
    for (Voice& v : m_voice) {
        if (v.active && !v.released)
            release_voice(v);
    }
}

void MusPlayer::reset_channels() {
    for (Channel& c : m_chan)
        c = Channel();
}

void MusPlayer::set_bend(int chan, int bend) {
    m_chan[chan].bend = static_cast<uint8_t>(bend);
    if (chan == kPercussionChannel)
        return;
    for (Voice& v : m_voice) {
        if (v.active && v.chan == chan)
            v.inc = freq_to_inc(note_freq(v.note, bend));
    }
}

// Runs every event due now, up to and including the group's trailing delay.
void MusPlayer::run_events() {
    // A malformed score of nothing but zero-delay events with looping on would otherwise spin forever.
    int guard = 4096;
    while (m_playing && m_delay_ticks == 0 && guard--) {
        if (at_end()) {
            if (m_looping) { m_pos = m_score; reset_channels(); all_notes_off(); continue; }
            m_playing = false;
            all_notes_off();
            return;
        }
        const uint8_t ev = *m_pos++;
        const bool last = (ev & 0x80) != 0;
        const int type = (ev >> 4) & 7;
        const int chan = ev & 15;
        switch (type) {
        case 0: // release note
            if (!at_end()) note_off(chan, *m_pos++ & 127);
            break;
        case 1: { // play note
            if (at_end()) break;
            const uint8_t b = *m_pos++;
            if ((b & 0x80) && !at_end())
                m_chan[chan].last_vel = *m_pos++ & 127;
            start_note(chan, b & 127, m_chan[chan].last_vel);
            break;
        }
        case 2: // pitch bend
            if (!at_end()) set_bend(chan, *m_pos++);
            break;
        case 3: { // system event
            if (at_end()) break;
            const int c = *m_pos++;
            if (c == 10 || c == 11) channel_notes_off(chan);
            break;
        }
        case 4: { // controller
            if (m_end - m_pos < 2) { m_pos = m_end; break; }
            const int c = m_pos[0];
            const int val = m_pos[1] & 127;
            m_pos += 2;
            if (c == 0) m_chan[chan].program = static_cast<uint8_t>(val);
            else if (c == 3) m_chan[chan].volume = static_cast<uint8_t>(val);
            else if (c == 5) m_chan[chan].expression = static_cast<uint8_t>(val);
            break;
        }
        case 5: // end of measure
            break;
        case 6: // score end
            m_pos = m_end;
            break;
        default:
            break;
        }
        if (last && type != 6) {
            int d = 0;
            while (!at_end()) {
                const uint8_t b = *m_pos++;
                d = (d << 7) | (b & 127);
                if (!(b & 0x80))
                    break;
            }
            m_delay_ticks = d;
        }
    }
}

template <MusPlayer::Shape S>
void MusPlayer::render_voice(Voice& v, float* out_l, float* out_r, int n, float gain) {
    uint32_t phase = v.phase;
    const uint32_t inc = v.inc;
    uint32_t rng = v.rng;
    float env = v.env;
    const float decay = v.decay;
    for (int i = 0; i < n; ++i) {
        const uint32_t np = phase + inc;
        int s;
        if constexpr (S == Noise) {
            if (np < phase)
                rng = rng * 1664525u + 1013904223u;
            s = static_cast<int8_t>(rng >> 24);
        } else {
            const int p = static_cast<int>(np >> 24);
            if constexpr (S == Square) s = p < 128 ? 112 : -112;
            else if constexpr (S == Pulse) s = p < 64 ? 112 : -112;
            else if constexpr (S == Tri) s = (p < 128 ? p : 255 - p) * 2 - 127;
            else s = p - 128;
        }
        phase = np;
        const float o = static_cast<float>(s) * gain * env;
        out_l[i] += o;
        out_r[i] += o;
        env *= decay;
    }
    v.phase = phase;
    v.rng = rng;
    v.env = env;
    if (env < kSilence)
        v.active = false;
}

void MusPlayer::render_voices(float* out_l, float* out_r, int n) {
    for (Voice& v : m_voice) {
        if (!v.active)
            continue;
        const Channel& c = m_chan[v.chan];
        const float amp = (static_cast<float>(v.vel) / 127.0f) * (static_cast<float>(c.volume) / 127.0f)
                          * (static_cast<float>(c.expression) / 127.0f);
        const float gain = m_master * kVoiceGain * amp * (1.0f / 128.0f);
        switch (v.shape) {
        case Square: render_voice<Square>(v, out_l, out_r, n, gain); break;
        case Pulse: render_voice<Pulse>(v, out_l, out_r, n, gain); break;
        case Tri: render_voice<Tri>(v, out_l, out_r, n, gain); break;
        case Saw: render_voice<Saw>(v, out_l, out_r, n, gain); break;
        case Noise: render_voice<Noise>(v, out_l, out_r, n, gain); break;
        }
    }
}

bool MusPlayer::any_voice_active() const {
    for (const Voice& v : m_voice) {
        if (v.active)
            return true;
    }
    return false;
}

bool MusPlayer::start(const void* mus_lump, bool looping) {
    const uint8_t* d = static_cast<const uint8_t*>(mus_lump);
    if (!d || std::memcmp(d, "MUS\x1a", 4) != 0)
        return false;
    const uint16_t len = rd16(d + 4);
    const uint16_t start_offset = rd16(d + 6);
    stop();
    reset_channels();
    m_score = d + start_offset;
    m_pos = m_score;
    m_end = m_score + len;
    m_looping = looping;
    m_paused = false;
    m_delay_ticks = 0;
    m_samples_to_tick = m_samples_per_tick;
    m_playing = true;
    run_events();
    return true;
}

void MusPlayer::stop() {
    m_playing = false;
    m_score = m_pos = m_end = nullptr;
    all_notes_off();
}

void MusPlayer::pause() {
    m_paused = true;
    all_notes_off();
}

void MusPlayer::resume() {
    m_paused = false;
}

void MusPlayer::set_volume(int volume) {
    m_master = kMasterScale * static_cast<float>(std::clamp(volume, 0, 127)) / 127.0f;
}

void MusPlayer::render(float* mix_l, float* mix_r, int n) {
    const bool sequencing = m_playing && !m_paused;
    if (!sequencing && !any_voice_active())
        return;
    int off = 0;
    while (off < n) {
        int seg = n - off;
        if (sequencing && m_playing && seg > m_samples_to_tick)
            seg = m_samples_to_tick;
        render_voices(mix_l + off, mix_r + off, seg);
        off += seg;
        if (sequencing && m_playing) {
            m_samples_to_tick -= seg;
            if (m_samples_to_tick == 0) {
                m_samples_to_tick = m_samples_per_tick;
                if (m_delay_ticks > 0)
                    --m_delay_ticks;
                if (m_delay_ticks == 0)
                    run_events();
            }
        }
    }
}

void MusPlayer::step(int ticks) {
    if (!m_playing || m_paused)
        return;
    for (; ticks > 0 && m_playing; --ticks) {
        for (Voice& v : m_voice) {
            if (!v.active)
                continue;
            v.env *= std::pow(v.decay, static_cast<float>(m_samples_per_tick));
            if (v.env < kSilence)
                v.active = false;
        }
        if (m_delay_ticks > 0)
            --m_delay_ticks;
        if (m_delay_ticks == 0)
            run_events();
    }
}

bool MusPlayer::lead(float* hz, float* level) const {
    const Voice* best = nullptr;
    float best_score = 0.0f;
    float best_level = 0.0f;
    for (const Voice& v : m_voice) {
        if (!v.active || v.released || v.chan == kPercussionChannel || v.shape == Noise)
            continue;
        const Channel& c = m_chan[v.chan];
        const float amp = (static_cast<float>(v.vel) / 127.0f) * (static_cast<float>(c.volume) / 127.0f)
                          * (static_cast<float>(c.expression) / 127.0f) * v.env;
        const float score = amp * (0.5f + static_cast<float>(v.note) / 127.0f);
        if (score > best_score) {
            best = &v;
            best_score = score;
            best_level = amp;
        }
    }
    if (!best || best_level < 0.05f)
        return false;
    *hz = note_freq(best->note, m_chan[best->chan].bend);
    *level = best_level > 1.0f ? 1.0f : best_level;
    return true;
}

void MusSource::fill(int16_t* pcm, size_t frames) {
    constexpr int kChunk = 128;
    float l[kChunk], r[kChunk];
    const uint8_t ch = m_channels ? m_channels : 1;
    while (frames > 0) {
        const int n = static_cast<int>(std::min<size_t>(frames, kChunk));
        std::memset(l, 0, sizeof(float) * static_cast<size_t>(n));
        std::memset(r, 0, sizeof(float) * static_cast<size_t>(n));
        m_player.render(l, r, n);
        for (int i = 0; i < n; ++i) {
            const int16_t o = static_cast<int16_t>(std::clamp(l[i] * 32767.0f, -32768.0f, 32767.0f));
            for (uint8_t c = 0; c < ch; ++c)
                *pcm++ = o;
        }
        frames -= static_cast<size_t>(n);
    }
}

} // namespace pico_toolset

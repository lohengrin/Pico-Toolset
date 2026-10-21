#pragma once

#include <cstddef>
#include <cstdint>

namespace pico_toolset {

// All fields are board wiring; see piezo_configs.h.
struct PiezoConfig {
    uint8_t pin;
    float   default_duty = 0.5f; // 0-1; 0.5 is loudest for a square wave
};

// One note of a melody: freq_hz == 0 is a rest.
struct PiezoNote {
    uint16_t freq_hz;
    uint16_t duration_ms;
};

// Piezo/buzzer on one PWM pin: a square wave at the requested frequency.
// Frequencies below 50 Hz switch the output off.
class Piezo {
public:
    void init(const PiezoConfig& config);

    // Starts a continuous tone (duty < 0 uses default_duty). Returns false if
    // the frequency was too low and the output was silenced instead.
    bool set_tone(uint32_t freq_hz, float duty = -1.0f);
    void stop();

    // Blocking melody; each note is followed by a short gap.
    void play(const PiezoNote* notes, size_t count);

private:
    PiezoConfig m_cfg{};
};

} // namespace pico_toolset

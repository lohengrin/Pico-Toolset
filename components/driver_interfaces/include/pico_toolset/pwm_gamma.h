#pragma once

#include <cmath>
#include <cstdint>

namespace pico_toolset {

// Perceptual brightness: maps 0-255 through gamma 2.8 onto a full 16-bit PWM counter (0-65535).
// The curve the Pimoroni-derived backlight drivers were validated with.
inline uint16_t pwm_gamma_level16(uint8_t brightness) {
    return static_cast<uint16_t>(std::pow(static_cast<float>(brightness) / 255.0f, 2.8f) * 65535.0f + 0.5f);
}

} // namespace pico_toolset

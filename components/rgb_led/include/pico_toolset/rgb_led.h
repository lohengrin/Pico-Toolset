#pragma once

#include <cstdint>

namespace pico_toolset {

// Configuration for a PWM-driven RGB LED (three separate GPIOs). Pins and
// polarity are board wiring, not driver behavior, so there are no defaults --
// see rgb_led_configs.h for validated per-board presets.
struct RgbLedConfig {
    uint8_t pin_r;
    uint8_t pin_g;
    uint8_t pin_b;
    bool    active_low; // true: the LED lights when its pin is LOW (common-anode / sinking wiring)
};

// RGB LED on three PWM pins, 8-bit-per-channel color with a gamma curve (2.8)
// so brightness looks linear to the eye. Each pin must be on a different PWM
// slice or share one coherently (the 16-bit wrap is configured per slice).
//
// Derived from Pimoroni's MIT-licensed RGBLED driver (same gamma/inversion
// behavior), rewritten without the Pimoroni dependency.
class RgbLed {
public:
    // Configures the three pins for PWM and switches the LED off.
    void init(const RgbLedConfig& config);

    // 0-255 per channel (0 = off).
    void set_rgb(uint8_t r, uint8_t g, uint8_t b);

private:
    void write(uint8_t pin, uint8_t value) const;

    RgbLedConfig m_cfg{};
};

} // namespace pico_toolset

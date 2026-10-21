#pragma once

#include <cstdint>

#include "hardware/i2c.h"

namespace pico_toolset {

// All fields are board wiring/jumper selection; see qwstpad_configs.h.
struct QwstPadConfig {
    i2c_inst_t* i2c;     // already i2c_init()'d by the caller (the bus is shared on QW/ST chains)
    uint8_t     addr;    // 0x21 default, 0x23 / 0x25 / 0x27 via the address jumpers
};

// Button bits of QwstPad::read_buttons().
namespace qwstpad_button {
inline constexpr uint16_t A = 1 << 0, B = 1 << 1, X = 1 << 2, Y = 1 << 3;
inline constexpr uint16_t Up = 1 << 4, Down = 1 << 5, Left = 1 << 6, Right = 1 << 7;
inline constexpr uint16_t Plus = 1 << 8, Minus = 1 << 9;
}

// Pimoroni QwSTPad: ten buttons and four LEDs behind a TCA9555 I2C
// expander. Up to four pads on one bus (one address each).
// Ported from Pimoroni's qwstpad-micropython (MIT).
class QwstPad {
public:
    static constexpr int kNumLeds = 4;

    // Configures the expander. False if the device does not answer.
    bool init(const QwstPadConfig& config);

    // Pressed buttons as qwstpad_button bits (1 = pressed). Returns 0 on bus error.
    [[nodiscard]] uint16_t read_buttons();

    // Low 4 bits = LEDs 1-4.
    void set_leds(uint8_t mask);
    // led 1..4.
    void set_led(int led, bool on);
    [[nodiscard]] uint8_t leds() const { return m_led_mask; }

    // Address jumper position (0-3) as an LED pattern: 0b0001, 0b0010, ...
    [[nodiscard]] uint8_t address_code() const;

private:
    bool write16(uint8_t reg, uint16_t value);
    bool read16(uint8_t reg, uint16_t& value);
    void update_leds();

    QwstPadConfig m_cfg{};
    uint8_t m_led_mask = 0;
};

} // namespace pico_toolset

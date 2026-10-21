// QwSTPad (TCA9555) driver, ported from Pimoroni's qwstpad-micropython (MIT).
#include "pico_toolset/qwstpad.h"

namespace pico_toolset {

namespace {

constexpr uint8_t kRegInput0 = 0x00;
constexpr uint8_t kRegOutput0 = 0x02;
constexpr uint8_t kRegPolarity0 = 0x04;
constexpr uint8_t kRegConfig0 = 0x06;

constexpr uint8_t kAddresses[4] = {0x21, 0x23, 0x25, 0x27};
constexpr uint32_t kTimeoutUs = 5000;

// Expander bit for each qwstpad_button bit (A B X Y U D L R + -).
constexpr uint8_t kButtonPins[10] = {0xE, 0xC, 0xF, 0xD, 0x1, 0x4, 0x2, 0x3, 0xB, 0x5};
// Expander bit for each LED.
constexpr uint8_t kLedPins[QwstPad::kNumLeds] = {0x6, 0x7, 0x9, 0xA};

} // namespace

bool QwstPad::write16(uint8_t reg, uint16_t value) {
    const uint8_t buf[3] = {reg, static_cast<uint8_t>(value & 0xFF), static_cast<uint8_t>(value >> 8)};
    return i2c_write_timeout_us(m_cfg.i2c, m_cfg.addr, buf, 3, false, kTimeoutUs) == 3;
}

bool QwstPad::read16(uint8_t reg, uint16_t& value) {
    uint8_t buf[2];
    if (i2c_write_timeout_us(m_cfg.i2c, m_cfg.addr, &reg, 1, true, kTimeoutUs) != 1) return false;
    if (i2c_read_timeout_us(m_cfg.i2c, m_cfg.addr, buf, 2, false, kTimeoutUs) != 2) return false;
    value = static_cast<uint16_t>(buf[0] | (buf[1] << 8));
    return true;
}

bool QwstPad::init(const QwstPadConfig& config) {
    m_cfg = config;
    // Buttons are inputs with inverted polarity (pressed reads 1); the four
    // LED pins are outputs, driven low = lit.
    if (!write16(kRegConfig0, 0b1111100100111111)) return false;
    write16(kRegPolarity0, 0b1111100000111111);
    write16(kRegOutput0, 0b0000011011000000);
    m_led_mask = 0;
    set_leds(address_code());
    return true;
}

uint16_t QwstPad::read_buttons() {
    uint16_t raw;
    if (!read16(kRegInput0, raw)) return 0;
    uint16_t out = 0;
    for (int i = 0; i < 10; ++i)
        if (raw & (1u << kButtonPins[i])) out |= 1u << i;
    return out;
}

void QwstPad::update_leds() {
    uint16_t out = 0;
    for (int i = 0; i < kNumLeds; ++i)
        if (!(m_led_mask & (1u << i))) out |= 1u << kLedPins[i];
    write16(kRegOutput0, out);
}

void QwstPad::set_leds(uint8_t mask) {
    m_led_mask = mask & 0x0F;
    update_leds();
}

void QwstPad::set_led(int led, bool on) {
    if (led < 1 || led > kNumLeds) return;
    const uint8_t bit = 1u << (led - 1);
    m_led_mask = on ? (m_led_mask | bit) : (m_led_mask & ~bit);
    update_leds();
}

uint8_t QwstPad::address_code() const {
    for (int i = 0; i < 4; ++i)
        if (kAddresses[i] == m_cfg.addr) return 1u << i;
    return 0;
}

} // namespace pico_toolset

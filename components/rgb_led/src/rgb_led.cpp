#include "pico_toolset/rgb_led.h"

#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include <cmath>

namespace pico_toolset {

namespace {

// gamma 2.8 on 8 bits, computed once (256 entries)
struct GammaTable {
    uint8_t v[256];
    GammaTable() {
        for (int i = 0; i < 256; ++i) {
            const int g = static_cast<int>(std::pow(i / 255.0f, 2.8f) * 255.0f + 0.5f);
            v[i] = static_cast<uint8_t>(i > 0 && g == 0 ? 1 : g); // a dim non-zero value must still light the LED
        }
    }
};

const GammaTable& gamma_table() {
    static const GammaTable table;
    return table;
}

void init_pin(uint8_t pin) {
    const uint slice = pwm_gpio_to_slice_num(pin);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, UINT16_MAX);
    pwm_init(slice, &cfg, true);
    gpio_set_function(pin, GPIO_FUNC_PWM);
}

} // namespace

void RgbLed::init(const RgbLedConfig& config) {
    m_cfg = config;
    init_pin(m_cfg.pin_r);
    init_pin(m_cfg.pin_g);
    init_pin(m_cfg.pin_b);
    set_rgb(0, 0, 0);
}

void RgbLed::write(uint8_t pin, uint8_t value) const {
    // gamma-corrected 8-bit value scaled to the 16-bit PWM range (255 * 255 = 65025)
    uint16_t level = static_cast<uint16_t>(gamma_table().v[value] * 255);
    if (m_cfg.active_low)
        level = UINT16_MAX - level;
    pwm_set_gpio_level(pin, level);
}

void RgbLed::set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    write(m_cfg.pin_r, r);
    write(m_cfg.pin_g, g);
    write(m_cfg.pin_b, b);
}

} // namespace pico_toolset

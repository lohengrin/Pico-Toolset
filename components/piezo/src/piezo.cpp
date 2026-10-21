#include "pico_toolset/piezo.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

namespace pico_toolset {

namespace {
constexpr uint32_t kMinFreqHz = 50;
constexpr uint32_t kNoteGapMs = 10;
}

void Piezo::init(const PiezoConfig& config) {
    m_cfg = config;
    gpio_set_function(m_cfg.pin, GPIO_FUNC_PWM);
    stop();
}

bool Piezo::set_tone(uint32_t freq_hz, float duty) {
    const uint slice = pwm_gpio_to_slice_num(m_cfg.pin);
    if (freq_hz < kMinFreqHz) {
        stop();
        return false;
    }
    if (duty < 0.0f) duty = m_cfg.default_duty;
    if (duty > 1.0f) duty = 1.0f;

    // Smallest divider that keeps the 16-bit wrap in range, for best resolution.
    const float sys = static_cast<float>(clock_get_hz(clk_sys));
    float div = sys / (static_cast<float>(freq_hz) * 65536.0f);
    if (div < 1.0f) div = 1.0f;
    const uint32_t wrap = static_cast<uint32_t>(sys / (div * static_cast<float>(freq_hz))) - 1;

    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, wrap);
    pwm_set_gpio_level(m_cfg.pin, static_cast<uint16_t>(duty * static_cast<float>(wrap)));
    pwm_set_enabled(slice, true);
    return true;
}

void Piezo::stop() {
    const uint slice = pwm_gpio_to_slice_num(m_cfg.pin);
    pwm_set_gpio_level(m_cfg.pin, 0);
    pwm_set_enabled(slice, false);
    gpio_set_function(m_cfg.pin, GPIO_FUNC_SIO);
    gpio_set_dir(m_cfg.pin, GPIO_OUT);
    gpio_put(m_cfg.pin, false);
    gpio_set_function(m_cfg.pin, GPIO_FUNC_PWM);
}

void Piezo::play(const PiezoNote* notes, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (notes[i].freq_hz) set_tone(notes[i].freq_hz);
        sleep_ms(notes[i].duration_ms);
        stop();
        sleep_ms(kNoteGapMs);
    }
}

} // namespace pico_toolset

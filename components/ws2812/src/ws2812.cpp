// WS2812/SK6812 chain over PIO; PIO program from pico-examples (BSD-3-Clause).
#include "pico_toolset/ws2812.h"

#include <cmath>

#include "hardware/clocks.h"
#include "ws2812.pio.h"

namespace pico_toolset {

bool Ws2812::init(const Ws2812Config& config) {
    if (config.count == 0 || config.count > kMaxLeds) return false;
    m_cfg = config;

#if PICO_PIO_USE_GPIO_BASE
    if (m_cfg.gpio_base >= 0) pio_set_gpio_base(m_cfg.pio, m_cfg.gpio_base);
#endif

    if (!pio_can_add_program(m_cfg.pio, &ws2812_program)) return false;
    pio_sm_claim(m_cfg.pio, m_cfg.sm);
    const uint offset = pio_add_program(m_cfg.pio, &ws2812_program);

    pio_gpio_init(m_cfg.pio, m_cfg.pin);
    pio_sm_set_consecutive_pindirs(m_cfg.pio, m_cfg.sm, m_cfg.pin, 1, true);

    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, m_cfg.pin);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    constexpr int kCyclesPerBit = ws2812_T1 + ws2812_T2 + ws2812_T3;
    sm_config_set_clkdiv(&c, static_cast<float>(clock_get_hz(clk_sys)) /
                                 (m_cfg.serial_hz * kCyclesPerBit));
    pio_sm_init(m_cfg.pio, m_cfg.sm, offset, &c);
    pio_sm_set_enabled(m_cfg.pio, m_cfg.sm, true);

    m_inited = true;
    clear();
    show();
    return true;
}

void Ws2812::set_rgb(size_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= m_cfg.count) return;
    m_pixels[index] = m_cfg.order == Ws2812Order::GRB
        ? (static_cast<uint32_t>(g) << 16) | (static_cast<uint32_t>(r) << 8) | b
        : (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

void Ws2812::set_hsv(size_t index, float h, float s, float v) {
    h = std::fmod(h, 360.0f);
    if (h < 0) h += 360.0f;
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    switch (static_cast<int>(h / 60.0f)) {
    case 0: r = c; g = x; break;
    case 1: r = x; g = c; break;
    case 2: g = c; b = x; break;
    case 3: g = x; b = c; break;
    case 4: r = x; b = c; break;
    default: r = c; b = x; break;
    }
    set_rgb(index, static_cast<uint8_t>((r + m) * 255.0f + 0.5f),
            static_cast<uint8_t>((g + m) * 255.0f + 0.5f),
            static_cast<uint8_t>((b + m) * 255.0f + 0.5f));
}

void Ws2812::clear() {
    for (size_t i = 0; i < m_cfg.count; ++i) m_pixels[i] = 0;
}

void Ws2812::show() {
    if (!m_inited) return;
    for (size_t i = 0; i < m_cfg.count; ++i) pio_sm_put_blocking(m_cfg.pio, m_cfg.sm, m_pixels[i] << 8);
}

} // namespace pico_toolset

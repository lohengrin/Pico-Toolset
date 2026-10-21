// FT6236 capacitive touch driver, ported from Pimoroni's Presto touch.py (MIT).
#include "pico_toolset/ft6236.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

namespace pico_toolset {

namespace {

constexpr uint kPollTimeoutUs = 50'000;
constexpr uint8_t kMaxErrors = 10;
constexpr uint8_t kBackoffPolls = 20;
constexpr uint kControllerMax = 480; // controller native range

// Event flag (top 2 bits of the X high byte): 0 down, 1 up, 2 contact, 3 none.
constexpr uint8_t kEventUp = 1;
constexpr uint8_t kEventNone = 3;

} // namespace

void Ft6236::init(const Ft6236Config& config) {
    m_cfg = config;
    i2c_init(m_cfg.i2c, m_cfg.i2c_freq_hz);
    gpio_set_function(m_cfg.pin_sda, GPIO_FUNC_I2C);
    gpio_set_function(m_cfg.pin_scl, GPIO_FUNC_I2C);
    gpio_pull_up(m_cfg.pin_sda);
    gpio_pull_up(m_cfg.pin_scl);
    if (m_cfg.pin_int != 0xFF) {
        gpio_init(m_cfg.pin_int);
        gpio_set_dir(m_cfg.pin_int, GPIO_IN);
        gpio_pull_up(m_cfg.pin_int);
    }
    m_t1 = m_t2 = TouchSample{};
    m_errors = m_skip = 0;
}

// A controller interrupted mid-transaction can hold SDA low and wedge the
// bus; clocking SCL lets it finish and release the line.
void Ft6236::recover_bus() {
    i2c_deinit(m_cfg.i2c);
    gpio_init(m_cfg.pin_scl);
    gpio_set_dir(m_cfg.pin_scl, GPIO_OUT);
    gpio_put(m_cfg.pin_scl, 1);
    gpio_init(m_cfg.pin_sda);
    gpio_set_dir(m_cfg.pin_sda, GPIO_IN);
    gpio_pull_up(m_cfg.pin_sda);
    for (int i = 0; i < 9 && !gpio_get(m_cfg.pin_sda); ++i) {
        gpio_put(m_cfg.pin_scl, 0);
        sleep_us(5);
        gpio_put(m_cfg.pin_scl, 1);
        sleep_us(5);
    }
    i2c_init(m_cfg.i2c, m_cfg.i2c_freq_hz);
    gpio_set_function(m_cfg.pin_sda, GPIO_FUNC_I2C);
    gpio_set_function(m_cfg.pin_scl, GPIO_FUNC_I2C);
}

void Ft6236::decode(const uint8_t* d, TouchSample& out) const {
    const uint8_t event = d[0] >> 6;
    uint32_t x = ((d[0] & 0x0F) << 8) | d[1];
    uint32_t y = ((d[2] & 0x0F) << 8) | d[3];
    x = x * m_cfg.width / kControllerMax;
    y = y * m_cfg.height / kControllerMax;
    if (m_cfg.rotate_180) {
        x = (m_cfg.width - 1) - x;
        y = (m_cfg.height - 1) - y;
    }
    out.raw_x = static_cast<uint16_t>(x);
    out.raw_y = static_cast<uint16_t>(y);
    out.pressed = event != kEventNone && event != kEventUp;
}

bool Ft6236::transfer() {
    uint8_t buf[15];
    const uint8_t reg = 0;
    if (i2c_write_blocking_until(m_cfg.i2c, m_cfg.addr, &reg, 1, true,
                                 make_timeout_time_us(kPollTimeoutUs)) != 1)
        return false;
    if (i2c_read_blocking_until(m_cfg.i2c, m_cfg.addr, buf, sizeof(buf), false,
                                make_timeout_time_us(kPollTimeoutUs)) != static_cast<int>(sizeof(buf)))
        return false;

    const uint8_t touches = buf[2] & 0x0F;
    m_t1.pressed = m_t2.pressed = false;
    for (uint8_t n = 0; n < touches && n < 2; ++n) {
        const uint8_t* d = &buf[3 + n * 6];
        decode(d, (d[2] >> 4) == 0 ? m_t1 : m_t2);
    }
    return true;
}

TouchSample Ft6236::read() {
    if (m_skip) {
        --m_skip;
        return m_t1;
    }
    const bool int_low = m_cfg.pin_int == 0xFF || !gpio_get(m_cfg.pin_int);
    if (!int_low && !m_t1.pressed && !m_t2.pressed) return m_t1;

    if (transfer()) {
        m_errors = 0;
    } else {
        // The controller NACKs occasionally; drop the sample and retry later
        // rather than stalling the caller's frame loop on every poll.
        if (m_errors < kMaxErrors) ++m_errors;
        m_skip = kBackoffPolls;
        if (m_errors > 1) recover_bus();
        m_t1.pressed = m_t2.pressed = false;
    }
    return m_t1;
}

} // namespace pico_toolset

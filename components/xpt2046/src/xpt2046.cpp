#include "pico_toolset/xpt2046.h"

#include "hardware/gpio.h"

namespace pico_toolset {

namespace {
// XPT2046 control byte format: S(1) A2A1A0(channel) MODE(12/8-bit)
// SER/DFR PD1PD0. 0xD0 = X-position channel, 0x90 = Y-position channel,
// both 12-bit differential with power-down-between-conversions -- the
// standard values most XPT2046 drivers use.
constexpr uint8_t kControlX = 0xD0;
constexpr uint8_t kControlY = 0x90;
} // namespace

void Xpt2046Touch::init(const Xpt2046Config& config) {
    m_spi = config.spi_instance;
    m_pin_cs = config.pin_cs;
    m_pin_irq = config.pin_irq;
    m_touch_baud = config.touch_freq_hz;

    gpio_init(m_pin_cs);
    gpio_set_dir(m_pin_cs, GPIO_OUT);
    gpio_put(m_pin_cs, 1);

    gpio_init(m_pin_irq);
    gpio_set_dir(m_pin_irq, GPIO_IN);
    gpio_pull_up(m_pin_irq);
}

uint16_t Xpt2046Touch::read_channel(uint8_t control_byte) {
    uint8_t tx[3] = {control_byte, 0x00, 0x00};
    uint8_t rx[3] = {0, 0, 0};
    spi_write_read_blocking(m_spi, tx, rx, 3);
    // 12-bit result sits in bits 14-3 of the two bytes after the control byte
    // (bit 15 is a leading don't-care/BUSY bit; low 3 bits are padding) --
    // standard XPT2046 response framing.
    return static_cast<uint16_t>(((rx[1] << 8) | rx[2]) >> 3);
}

Xpt2046Touch::RawSample Xpt2046Touch::read() {
    if (gpio_get(m_pin_irq)) {
        return {}; // active-low: high = not pressed, skip the SPI transaction
    }

    spi_set_baudrate(m_spi, m_touch_baud);
    gpio_put(m_pin_cs, 0);
    uint16_t x = read_channel(kControlX);
    uint16_t y = read_channel(kControlY);
    gpio_put(m_pin_cs, 1);

    return RawSample{true, x, y};
}

} // namespace pico_toolset

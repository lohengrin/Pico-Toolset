#include "pico_toolset/xpt2046.h"

#include "hardware/gpio.h"

namespace pico_toolset {

void Xpt2046Touch::init(const Xpt2046Config& config) {
    m_spi = config.spi_instance;
    m_pin_cs = config.pin_cs;
    m_pin_irq = config.pin_irq;
    m_touch_baud = config.touch_freq_hz;
    m_pressure_threshold = config.pressure_threshold;
    m_resolution = config.resolution;
    m_power_mode = config.power_mode;
    m_touch_reference = config.touch_reference;

    gpio_init(m_pin_cs);
    gpio_set_dir(m_pin_cs, GPIO_OUT);
    gpio_put(m_pin_cs, 1);

    // PENIRQ is configured (when wired) but no longer used to gate read() --
    // see read()'s doc comment for why: confirmed on real hardware (2026-09)
    // that some boards leave it unconnected/non-functional despite the touch
    // layer itself working fine over SPI. 255 = not wired at all.
    if (m_pin_irq != 255) {
        gpio_init(m_pin_irq);
        gpio_set_dir(m_pin_irq, GPIO_IN);
        gpio_pull_up(m_pin_irq);
    }
}

uint8_t Xpt2046Touch::build_control_byte(Xpt2046Channel channel, Xpt2046Reference reference) const {
    constexpr uint8_t kStartBit = 0x80;
    const uint8_t channel_bits = static_cast<uint8_t>(static_cast<uint8_t>(channel) << 4);
    const uint8_t mode_bit = (m_resolution == Xpt2046Resolution::Bits8) ? 0x08 : 0x00;
    const uint8_t ser_dfr_bit = (reference == Xpt2046Reference::SingleEnded) ? 0x04 : 0x00;
    const uint8_t pd_bits = static_cast<uint8_t>(m_power_mode) & 0x03;
    return static_cast<uint8_t>(kStartBit | channel_bits | mode_bit | ser_dfr_bit | pd_bits);
}

uint16_t Xpt2046Touch::convert(uint8_t control_byte) {
    uint8_t tx[3] = {control_byte, 0x00, 0x00};
    uint8_t rx[3] = {0, 0, 0};
    spi_write_read_blocking(m_spi, tx, rx, 3);
    const uint16_t raw16 = static_cast<uint16_t>((rx[1] << 8) | rx[2]);
    // Bit 15 (rx[1]'s MSB) is a leading don't-care; the real result sits
    // just below it. 12-bit mode: bits 14-3 (shift 3). 8-bit mode: bits
    // 14-7 (shift 7) -- normalized back onto the same 0-4095 scale by
    // shifting the other way by 4, so callers never need to branch on
    // `resolution` themselves.
    if (m_resolution == Xpt2046Resolution::Bits8)
        return static_cast<uint16_t>((raw16 >> 7) << 4);
    return static_cast<uint16_t>(raw16 >> 3);
}

uint16_t Xpt2046Touch::read_channel(Xpt2046Channel channel) {
    const bool is_touch_channel = channel == Xpt2046Channel::X || channel == Xpt2046Channel::Y ||
                                   channel == Xpt2046Channel::Z1 || channel == Xpt2046Channel::Z2;
    const Xpt2046Reference reference = is_touch_channel ? m_touch_reference : Xpt2046Reference::SingleEnded;
    const uint8_t control_byte = build_control_byte(channel, reference);

    spi_set_baudrate(m_spi, m_touch_baud);
    gpio_put(m_pin_cs, 0);
    const uint16_t value = convert(control_byte);
    gpio_put(m_pin_cs, 1);
    return value;
}

Xpt2046Touch::RawSample Xpt2046Touch::read() {
    // Pressure-based detection, not PENIRQ -- see the class doc comment.
    // Z = (4095-Z2)+Z1 is the standard XPT2046/ADS7846 touch-pressure
    // formula (low when untouched -- Z1 near 0, Z2 near full-scale -- and
    // large under finger/stylus pressure).
    const uint8_t control_z1 = build_control_byte(Xpt2046Channel::Z1, m_touch_reference);
    const uint8_t control_z2 = build_control_byte(Xpt2046Channel::Z2, m_touch_reference);
    const uint8_t control_x = build_control_byte(Xpt2046Channel::X, m_touch_reference);
    const uint8_t control_y = build_control_byte(Xpt2046Channel::Y, m_touch_reference);

    spi_set_baudrate(m_spi, m_touch_baud);
    gpio_put(m_pin_cs, 0);
    const uint16_t z1 = convert(control_z1);
    const uint16_t z2 = convert(control_z2);
    int32_t z = (4095 - static_cast<int32_t>(z2)) + static_cast<int32_t>(z1);
    if (z < 0)
        z = -z;

    if (z < m_pressure_threshold) {
        gpio_put(m_pin_cs, 1);
        return {};
    }

    const uint16_t x = convert(control_x);
    const uint16_t y = convert(control_y);
    gpio_put(m_pin_cs, 1);

    return RawSample{true, x, y};
}

} // namespace pico_toolset

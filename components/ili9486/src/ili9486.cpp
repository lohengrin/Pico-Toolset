#include "pico_toolset/ili9486.h"

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#include <array>

namespace pico_toolset {

namespace {

// ILI9486 command bytes used here.
constexpr uint8_t kCmdSleepOut = 0x11;
constexpr uint8_t kCmdDisplayOn = 0x29;
constexpr uint8_t kCmdCaset = 0x2A;
constexpr uint8_t kCmdPaset = 0x2B;
constexpr uint8_t kCmdRamwr = 0x2C;
constexpr uint8_t kCmdMadctl = 0x36;
constexpr uint8_t kCmdDisFunCtrl = 0xB6;

// MADCTL/Display-Function-Control pair for landscape (480x320): MV=1
// (row/column exchange) + BGR=1. If red/blue come out swapped, clear BGR
// (0x28 -> 0x20).
constexpr uint8_t kMadctlLandscape = 0x28;
constexpr uint8_t kDisFunLandscape = 0x62;

// One entry of the power-on register sequence: a command plus its data
// bytes. Panel-specific tuning values, transcribed from the vendor reference
// driver -- a "standard" ILI9486 sequence produces a blank panel.
struct InitCommand {
    uint8_t cmd;
    uint8_t len;
    std::array<uint8_t, 15> data;
};

constexpr std::array<InitCommand, 16> kInitSequence = {{
    {0xF9, 2, {0x00, 0x08}},
    {0xC0, 2, {0x19, 0x1A}},                   // VREG1OUT positive / VREG2OUT negative
    {0xC1, 2, {0x45, 0x00}},                   // VGH/VGL, VGH >= 14V
    {0xC2, 1, {0x33}},                         // normal-mode drive strength
    {0xC5, 2, {0x00, 0x28}},                   // VCM_REG[7:0], must stay <= 0x80
    {0xB1, 2, {0xA0, 0x11}},                   // frame rate: 0xA0 = 62Hz
    {0xB4, 1, {0x02}},                         // 2-dot frame mode
    {0xB6, 3, {0x00, 0x42, 0x3B}},             // display function control
    {0xB7, 1, {0x07}},
    {0xE0, 15, {0x1F, 0x25, 0x22, 0x0B, 0x06, 0x0A, 0x4E, 0xC6, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {0xE1, 15, {0x1F, 0x3F, 0x3F, 0x0F, 0x1F, 0x0F, 0x46, 0x49, 0x31, 0x05, 0x09, 0x03, 0x1C, 0x1A, 0x00}},
    {0xF1, 8, {0x36, 0x04, 0x00, 0x3C, 0x0F, 0x0F, 0xA4, 0x02}},
    {0xF2, 9, {0x18, 0xA3, 0x12, 0x02, 0x32, 0x12, 0xFF, 0x32, 0x00}},
    {0xF4, 5, {0x40, 0x00, 0x08, 0x91, 0x04}},
    {0xF8, 2, {0x21, 0x04}},
    {0x3A, 1, {0x55}},                         // 16 bits/pixel (RGB565)
}};

} // namespace

bool Ili9486::init(const Ili9486Config& config) {
    m_spi = config.spi_instance;
    m_pin_cs = config.pin_cs;
    m_pin_dc = config.pin_dc;
    m_pin_rst = config.pin_rst;
    m_pin_backlight = config.pin_backlight;
    m_command_baud = config.spi_freq_hz;
    m_pixel_baud = config.pixel_freq_hz;
    m_use_dma = config.use_dma && config.pixel_freq_hz <= 80'000'000;

    spi_init(m_spi, m_command_baud);
    gpio_set_function(config.pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(config.pin_mosi, GPIO_FUNC_SPI);
    gpio_set_function(config.pin_miso, GPIO_FUNC_SPI);

    gpio_init(m_pin_cs);
    gpio_set_dir(m_pin_cs, GPIO_OUT);
    gpio_put(m_pin_cs, 1);

    gpio_init(m_pin_dc);
    gpio_set_dir(m_pin_dc, GPIO_OUT);
    gpio_put(m_pin_dc, 1);

    gpio_init(m_pin_rst);
    gpio_set_dir(m_pin_rst, GPIO_OUT);

    if (m_pin_backlight != 255) {
        gpio_init(m_pin_backlight);
        gpio_set_dir(m_pin_backlight, GPIO_OUT);
        gpio_put(m_pin_backlight, 1);
        gpio_set_function(m_pin_backlight, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(m_pin_backlight);
        pwm_set_wrap(slice, 255);
        pwm_set_chan_level(slice, pwm_gpio_to_channel(m_pin_backlight), 255);
        pwm_set_enabled(slice, true);
    }

    if (m_use_dma) {
        if (config.dma_channel >= 0) {
            dma_channel_claim(static_cast<uint>(config.dma_channel));
            m_dma_chan = config.dma_channel;
        } else {
            m_dma_chan = dma_claim_unused_channel(true);
        }
    }

    // Hardware reset with the vendor reference's generous 500 ms timings.
    gpio_put(m_pin_rst, 1);
    sleep_ms(500);
    gpio_put(m_pin_rst, 0);
    sleep_ms(500);
    gpio_put(m_pin_rst, 1);
    sleep_ms(500);

    for (const InitCommand& entry : kInitSequence) {
        write_command(entry.cmd);
        for (uint8_t i = 0; i < entry.len; ++i)
            write_data_byte(entry.data[i]);
    }

    // Scan direction / orientation: display function control first, then MADCTL.
    write_command(kCmdDisFunCtrl);
    write_data_byte(0x00);
    write_data_byte(kDisFunLandscape);

    write_command(kCmdMadctl);
    write_data_byte(kMadctlLandscape);
    sleep_ms(200);

    write_command(kCmdSleepOut);
    sleep_ms(120);

    write_command(kCmdDisplayOn);
    return true;
}

void Ili9486::claim_bus() {
    spi_set_baudrate(m_spi, m_command_baud);
}

void Ili9486::write_command(uint8_t cmd) {
    gpio_put(m_pin_dc, 0);
    gpio_put(m_pin_cs, 0);
    spi_write_blocking(m_spi, &cmd, 1);
    gpio_put(m_pin_cs, 1);
}

void Ili9486::write_data_byte(uint8_t data) {
    const uint8_t buf[2] = {0x00, data};
    gpio_put(m_pin_dc, 1);
    gpio_put(m_pin_cs, 0);
    spi_write_blocking(m_spi, buf, 2);
    gpio_put(m_pin_cs, 1);
}

void Ili9486::set_window(int x0, int y0, int x1, int y1) {
    claim_bus();

    write_command(kCmdCaset);
    write_data_byte(static_cast<uint8_t>(x0 >> 8));
    write_data_byte(static_cast<uint8_t>(x0 & 0xFF));
    write_data_byte(static_cast<uint8_t>(x1 >> 8));
    write_data_byte(static_cast<uint8_t>(x1 & 0xFF));

    write_command(kCmdPaset);
    write_data_byte(static_cast<uint8_t>(y0 >> 8));
    write_data_byte(static_cast<uint8_t>(y0 & 0xFF));
    write_data_byte(static_cast<uint8_t>(y1 >> 8));
    write_data_byte(static_cast<uint8_t>(y1 & 0xFF));

    write_command(kCmdRamwr);

    // Switch to the pixel clock BEFORE asserting CS: spi_set_baudrate()
    // disables/re-enables the peripheral and must not fire while the shift
    // register is latching (real-hardware horizontal-shift bug, see TOM6809).
    spi_set_baudrate(m_spi, m_pixel_baud);

    gpio_put(m_pin_dc, 1);
    gpio_put(m_pin_cs, 0);
}

void Ili9486::end_write() {
    gpio_put(m_pin_cs, 1);
}

void Ili9486::write_pixels(std::span<const uint16_t> pixels) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(pixels.data());
    size_t n = pixels.size_bytes();

    if (m_use_dma && m_dma_chan >= 0) {
        dma_channel_config cfg = dma_channel_get_default_config(m_dma_chan);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
        channel_config_set_read_increment(&cfg, true);
        channel_config_set_write_increment(&cfg, false);
        channel_config_set_dreq(&cfg, spi_get_dreq(m_spi, true));
        dma_channel_configure(m_dma_chan, &cfg,
                              &spi_get_hw(m_spi)->dr,
                              bytes, n, true);
        dma_channel_wait_for_finish_blocking(m_dma_chan);
    } else {
        spi_write_blocking(m_spi, bytes, n);
    }
}

void Ili9486::fill_solid(uint16_t rgb565) {
    uint16_t wire = static_cast<uint16_t>((rgb565 << 8) | (rgb565 >> 8));
    std::array<uint16_t, kWidth> row;
    row.fill(wire);

    set_window(0, 0, kWidth - 1, kHeight - 1);
    for (int y = 0; y < kHeight; ++y)
        write_pixels(row);
    end_write();
}

void Ili9486::set_backlight(uint8_t brightness) {
    if (m_pin_backlight != 255) {
        pwm_set_chan_level(pwm_gpio_to_slice_num(m_pin_backlight),
                           pwm_gpio_to_channel(m_pin_backlight), brightness);
    }
}

} // namespace pico_toolset
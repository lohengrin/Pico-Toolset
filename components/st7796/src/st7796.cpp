#include "pico_toolset/st7796.h"

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#include <cmath>

namespace pico_toolset {

namespace {

// ST7796U command bytes used here.
constexpr uint8_t kCmdSwReset = 0x01;
constexpr uint8_t kCmdSlpOut = 0x11;
constexpr uint8_t kCmdDispOn = 0x29;
constexpr uint8_t kCmdCaset = 0x2A;
constexpr uint8_t kCmdRaset = 0x2B;
constexpr uint8_t kCmdRamwr = 0x2C;
constexpr uint8_t kCmdMadctl = 0x36;
constexpr uint8_t kCmdColmod = 0x3A;
constexpr uint8_t kCmdInvCtr = 0xB4;   // column inversion
constexpr uint8_t kCmdDfuncCtr = 0xB6; // display function control
constexpr uint8_t kCmdPwctr2 = 0xC1;   // power control 2
constexpr uint8_t kCmdPwctr3 = 0xC2;   // power control 3
constexpr uint8_t kCmdVmctr1 = 0xC5;   // VCOM control
constexpr uint8_t kCmdDoca = 0xE8;     // display output ctrl adjust
constexpr uint8_t kCmdGmctrp1 = 0xE0;  // gamma "+"
constexpr uint8_t kCmdGmctrn1 = 0xE1;  // gamma "-"
constexpr uint8_t kCmdCmdSet = 0xF0;   // command set control (extended command enable/disable)

} // namespace

bool St7796::init(const St7796Config& config) {
    m_spi = config.spi_instance;
    m_pin_cs = config.pin_cs;
    m_pin_dc = config.pin_dc;
    m_pin_backlight = config.pin_backlight;
    m_width = config.width;
    m_height = config.height;
    m_col_offset = config.col_offset;
    m_row_offset = config.row_offset;
    m_spi_freq_hz = config.spi_freq_hz;
    m_use_dma = config.use_dma && config.spi_freq_hz <= 80'000'000;

    spi_init(m_spi, m_spi_freq_hz);
    gpio_set_function(config.pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(config.pin_mosi, GPIO_FUNC_SPI);
    gpio_set_function(config.pin_miso, GPIO_FUNC_SPI);

    gpio_init(m_pin_dc);
    gpio_set_dir(m_pin_dc, GPIO_OUT);

    gpio_init(m_pin_cs);
    gpio_set_dir(m_pin_cs, GPIO_OUT);
    gpio_put(m_pin_cs, 1);

    if (config.pin_rst != 255) {
        gpio_init(config.pin_rst);
        gpio_set_dir(config.pin_rst, GPIO_OUT);
    }

    if (m_pin_backlight != 255) {
        pwm_config cfg = pwm_get_default_config();
        pwm_set_wrap(pwm_gpio_to_slice_num(m_pin_backlight), 65535);
        pwm_init(pwm_gpio_to_slice_num(m_pin_backlight), &cfg, true);
        gpio_set_function(m_pin_backlight, GPIO_FUNC_PWM);
        set_backlight(0); // avoid a garbage-frame flash before the panel is configured
    }

    if (m_use_dma) {
        if (config.dma_channel >= 0) {
            dma_channel_claim(static_cast<uint>(config.dma_channel));
            m_dma_chan = config.dma_channel;
        } else {
            m_dma_chan = dma_claim_unused_channel(true);
        }
    }

    // Hardware reset sequence (skipped if the panel has no dedicated reset
    // pin -- rely on the SWRESET command below instead). Timing matches
    // Ili9486's validated reset sequence for the same board family.
    if (config.pin_rst != 255) {
        gpio_put(config.pin_rst, true);
        sleep_ms(5);
        gpio_put(config.pin_rst, false);
        sleep_ms(20);
        gpio_put(config.pin_rst, true);
        sleep_ms(120);
    }

    write_command(kCmdSwReset);
    sleep_ms(120);
    write_command(kCmdSlpOut);
    sleep_ms(120);

    // Register sequence below is ST7796U's own manufacturer-recommended
    // init (ported from a widely-used, real-panel-validated reference --
    // TFT_eSPI's ST7796 driver -- not invented from the datasheet alone).
    // Enable the extended command set (partI+partII) to reach the
    // power/gamma registers that follow; disabled again at the end.
    const uint8_t cmdset_en1[] = {0xC3};
    write_command(kCmdCmdSet, cmdset_en1, 1);
    const uint8_t cmdset_en2[] = {0x96};
    write_command(kCmdCmdSet, cmdset_en2, 1);

    const uint8_t colmod[] = {0x55}; // 16 bits per pixel, both interfaces
    write_command(kCmdColmod, colmod, 1);

    const uint8_t invctr[] = {0x01}; // 1-dot inversion
    write_command(kCmdInvCtr, invctr, 1);

    const uint8_t dfunctr[] = {0x80, 0x02, 0x3B};
    write_command(kCmdDfuncCtr, dfunctr, 3);

    const uint8_t doca[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
    write_command(kCmdDoca, doca, 8);

    const uint8_t pwctr2[] = {0x06};
    write_command(kCmdPwctr2, pwctr2, 1);
    const uint8_t pwctr3[] = {0xA7};
    write_command(kCmdPwctr3, pwctr3, 1);
    const uint8_t vmctr1[] = {0x18};
    write_command(kCmdVmctr1, vmctr1, 1);

    sleep_ms(120);

    const uint8_t gmctrp1[] = {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54,
                                0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B};
    write_command(kCmdGmctrp1, gmctrp1, 14);
    const uint8_t gmctrn1[] = {0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43,
                                0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B};
    write_command(kCmdGmctrn1, gmctrn1, 14);

    sleep_ms(120);

    const uint8_t cmdset_dis1[] = {0x3C};
    write_command(kCmdCmdSet, cmdset_dis1, 1);
    const uint8_t cmdset_dis2[] = {0x69};
    write_command(kCmdCmdSet, cmdset_dis2, 1);

    sleep_ms(120);

    write_command(kCmdDispOn);
    sleep_ms(100);

    const uint8_t madctl[] = {config.madctl};
    write_command(kCmdMadctl, madctl, 1);

    if (m_pin_backlight != 255) {
        sleep_ms(50); // let the init sequence settle before lighting up
        set_backlight(255);
    }

    return true;
}

void St7796::write_command(uint8_t cmd, const uint8_t* data, size_t len) {
    gpio_put(m_pin_dc, 0);
    gpio_put(m_pin_cs, 0);
    spi_write_blocking(m_spi, &cmd, 1);

    if (data != nullptr && len > 0) {
        gpio_put(m_pin_dc, 1);
        spi_write_blocking(m_spi, data, len);
    }

    gpio_put(m_pin_cs, 1);
}

void St7796::set_window(int x0, int y0, int x1, int y1) {
    m_spi_freq_actual_hz = spi_set_baudrate(m_spi, m_spi_freq_hz);

    const uint16_t xs = static_cast<uint16_t>(x0 + m_col_offset);
    const uint16_t xe = static_cast<uint16_t>(x1 + m_col_offset);
    const uint16_t ys = static_cast<uint16_t>(y0 + m_row_offset);
    const uint16_t ye = static_cast<uint16_t>(y1 + m_row_offset);

    const uint8_t caset[4] = {static_cast<uint8_t>(xs >> 8), static_cast<uint8_t>(xs & 0xFF),
                               static_cast<uint8_t>(xe >> 8), static_cast<uint8_t>(xe & 0xFF)};
    write_command(kCmdCaset, caset, 4);

    const uint8_t raset[4] = {static_cast<uint8_t>(ys >> 8), static_cast<uint8_t>(ys & 0xFF),
                               static_cast<uint8_t>(ye >> 8), static_cast<uint8_t>(ye & 0xFF)};
    write_command(kCmdRaset, raset, 4);

    gpio_put(m_pin_dc, 0);
    gpio_put(m_pin_cs, 0);
    const uint8_t ramwr = kCmdRamwr;
    spi_write_blocking(m_spi, &ramwr, 1);
    gpio_put(m_pin_dc, 1);
    // CS stays asserted; pixel data follows in the same burst (write_pixels()/end_write()).
}

void St7796::end_write() {
    gpio_put(m_pin_cs, 1);
}

void St7796::start_pixels_dma(std::span<const uint16_t> pixels) {
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
    } else {
        // No async path available: transfer synchronously right here.
        // pixels_busy() correctly reports false afterwards and
        // finish_pixels_dma() is a no-op, so the start/poll/finish trio
        // still behaves correctly for callers that use it unconditionally.
        spi_write_blocking(m_spi, bytes, n);
    }
}

bool St7796::pixels_busy() const {
    return m_use_dma && m_dma_chan >= 0 && dma_channel_is_busy(m_dma_chan);
}

void St7796::finish_pixels_dma() {
    if (!(m_use_dma && m_dma_chan >= 0))
        return; // start_pixels_dma()'s synchronous fallback already fully drained the bus.

    // See pico_toolset_ili9486's Ili9486::finish_pixels_dma() for why this
    // drain is required before handing the bus to a shared-bus device (e.g.
    // XPT2046 touch): DMA finishing only guarantees the TX FIFO emptied,
    // not that the RX FIFO is drained or the last byte has finished
    // shifting out (BSY).
    while (spi_is_readable(m_spi))
        (void) spi_get_hw(m_spi)->dr;
    while (spi_get_hw(m_spi)->sr & SPI_SSPSR_BSY_BITS)
        tight_loop_contents();
    while (spi_is_readable(m_spi))
        (void) spi_get_hw(m_spi)->dr;
}

void St7796::write_pixels(std::span<const uint16_t> pixels) {
    start_pixels_dma(pixels);
    while (pixels_busy())
        tight_loop_contents();
    finish_pixels_dma();
}

void St7796::fill_solid(uint16_t rgb565) {
    uint16_t wire = static_cast<uint16_t>((rgb565 << 8) | (rgb565 >> 8));
    uint16_t row[480]; // widest validated panel geometry so far
    for (uint16_t& px : row) px = wire;

    set_window(0, 0, m_width - 1, m_height - 1);
    for (uint16_t y = 0; y < m_height; ++y)
        write_pixels(std::span<const uint16_t>(row, m_width));
    end_write();
}

void St7796::set_backlight(uint8_t brightness) {
    if (m_pin_backlight == 255) return;
    // Gamma-correct the 0-255 input onto the PWM counter's 0-65535 range --
    // matches St7789::set_backlight()'s perceptual brightness curve.
    float gamma = 2.8f;
    uint16_t value = static_cast<uint16_t>(std::pow(static_cast<float>(brightness) / 255.0f, gamma) * 65535.0f + 0.5f);
    pwm_set_gpio_level(m_pin_backlight, value);
}

} // namespace pico_toolset

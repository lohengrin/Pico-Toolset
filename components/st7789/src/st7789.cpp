#include "pico_toolset/st7789.h"

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#include <cmath>

namespace pico_toolset {

namespace {

// ST7789 command bytes used here.
constexpr uint8_t kCmdSwReset = 0x01;
constexpr uint8_t kCmdTeOff = 0x34;
constexpr uint8_t kCmdTeOn = 0x35;
constexpr uint8_t kCmdMadctl = 0x36;
constexpr uint8_t kCmdColmod = 0x3A;
constexpr uint8_t kCmdGctrl = 0xB7;
constexpr uint8_t kCmdVcoms = 0xBB;
constexpr uint8_t kCmdLcmctrl = 0xC0;
constexpr uint8_t kCmdVdvvrhen = 0xC2;
constexpr uint8_t kCmdVrhs = 0xC3;
constexpr uint8_t kCmdVdvs = 0xC4;
constexpr uint8_t kCmdFrctrl2 = 0xC6;
constexpr uint8_t kCmdPwctrl1 = 0xD0;
constexpr uint8_t kCmdPorctrl = 0xB2;
constexpr uint8_t kCmdGmctrp1 = 0xE0;
constexpr uint8_t kCmdGmctrn1 = 0xE1;
constexpr uint8_t kCmdInvOff = 0x20;
constexpr uint8_t kCmdInvOn = 0x21;
constexpr uint8_t kCmdSlpOut = 0x11;
constexpr uint8_t kCmdDispOn = 0x29;
constexpr uint8_t kCmdCaset = 0x2A;
constexpr uint8_t kCmdRaset = 0x2B;
constexpr uint8_t kCmdRamwr = 0x2C;

} // namespace

bool St7789::init(const St7789Config& config) {
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

    gpio_init(m_pin_dc);
    gpio_set_dir(m_pin_dc, GPIO_OUT);

    gpio_init(m_pin_cs);
    gpio_set_dir(m_pin_cs, GPIO_OUT);
    gpio_put(m_pin_cs, 1);

    gpio_init(config.pin_reset);
    gpio_set_dir(config.pin_reset, GPIO_OUT);

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

    // Panel reset sequence -- matches the validated consumer driver's timing.
    gpio_put(config.pin_reset, true);
    sleep_ms(5);
    gpio_put(config.pin_reset, false);
    sleep_ms(20);
    gpio_put(config.pin_reset, true);

    write_command(kCmdSwReset);
    sleep_ms(150);

    write_command(config.tearing_effect_on ? kCmdTeOn : kCmdTeOff);
    const uint8_t colmod[] = {0x05}; // 16 bits per pixel
    write_command(kCmdColmod, colmod, 1);

    const uint8_t porctrl[] = {0x0c, 0x0c, 0x00, 0x33, 0x33};
    write_command(kCmdPorctrl, porctrl, 5);
    const uint8_t lcmctrl[] = {0x2c};
    write_command(kCmdLcmctrl, lcmctrl, 1);
    const uint8_t vdvvrhen[] = {0x01};
    write_command(kCmdVdvvrhen, vdvvrhen, 1);
    const uint8_t vrhs[] = {0x12};
    write_command(kCmdVrhs, vrhs, 1);
    const uint8_t vdvs[] = {0x20};
    write_command(kCmdVdvs, vdvs, 1);
    const uint8_t pwctrl1[] = {0xa4, 0xa1};
    write_command(kCmdPwctrl1, pwctrl1, 2);
    const uint8_t frctrl2[] = {0x0f};
    write_command(kCmdFrctrl2, frctrl2, 1);

    // Gamma/VCOM tuning tables: the 320x240 variant validated on the
    // CrowPanel PICO HMI 2.8". A different panel geometry (e.g. 240x240)
    // needs its own validated table -- add it as a second branch here (keyed
    // off config.width/height, same as the consumer driver this was ported
    // from) only once actually bench-tested; don't invent one speculatively.
    if (config.width == 320 && config.height == 240) {
        const uint8_t gctrl[] = {0x35};
        write_command(kCmdGctrl, gctrl, 1);
        const uint8_t vcoms[] = {0x1f};
        write_command(kCmdVcoms, vcoms, 1);
        const uint8_t gmctrp1[] = {0xD0, 0x08, 0x11, 0x08, 0x0C, 0x15, 0x39, 0x33,
                                    0x50, 0x36, 0x13, 0x14, 0x29, 0x2D};
        write_command(kCmdGmctrp1, gmctrp1, 14);
        const uint8_t gmctrn1[] = {0xD0, 0x08, 0x10, 0x08, 0x06, 0x06, 0x39, 0x44,
                                    0x51, 0x0B, 0x16, 0x14, 0x2F, 0x31};
        write_command(kCmdGmctrn1, gmctrn1, 14);
    }

    write_command(config.inversion_on ? kCmdInvOn : kCmdInvOff);
    write_command(kCmdSlpOut);
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

void St7789::write_command(uint8_t cmd, const uint8_t* data, size_t len) {
    gpio_put(m_pin_dc, 0);
    gpio_put(m_pin_cs, 0);
    spi_write_blocking(m_spi, &cmd, 1);

    if (data != nullptr && len > 0) {
        gpio_put(m_pin_dc, 1);
        spi_write_blocking(m_spi, data, len);
    }

    gpio_put(m_pin_cs, 1);
}

void St7789::set_window(int x0, int y0, int x1, int y1) {
    spi_set_baudrate(m_spi, m_spi_freq_hz);

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

void St7789::end_write() {
    gpio_put(m_pin_cs, 1);
}

void St7789::start_pixels_dma(std::span<const uint16_t> pixels) {
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

bool St7789::pixels_busy() const {
    return m_use_dma && m_dma_chan >= 0 && dma_channel_is_busy(m_dma_chan);
}

void St7789::finish_pixels_dma() {
    if (!(m_use_dma && m_dma_chan >= 0))
        return; // start_pixels_dma()'s synchronous fallback already fully drained the bus.

    // See pico_toolset_ili9486's Ili9486::finish_pixels_dma() for why this
    // drain is required before handing the bus to a shared-bus device (e.g.
    // XPT2046 touch or an SD card on the CrowPanel PICO HMI 2.8"): DMA
    // finishing only guarantees the TX FIFO emptied, not that the RX FIFO is
    // drained or the last byte has finished shifting out (BSY).
    while (spi_is_readable(m_spi))
        (void) spi_get_hw(m_spi)->dr;
    while (spi_get_hw(m_spi)->sr & SPI_SSPSR_BSY_BITS)
        tight_loop_contents();
    while (spi_is_readable(m_spi))
        (void) spi_get_hw(m_spi)->dr;
}

void St7789::write_pixels(std::span<const uint16_t> pixels) {
    start_pixels_dma(pixels);
    while (pixels_busy())
        tight_loop_contents();
    finish_pixels_dma();
}

void St7789::fill_solid(uint16_t rgb565) {
    uint16_t wire = static_cast<uint16_t>((rgb565 << 8) | (rgb565 >> 8));
    uint16_t row[320]; // widest validated panel geometry so far
    for (uint16_t& px : row) px = wire;

    set_window(0, 0, m_width - 1, m_height - 1);
    for (uint16_t y = 0; y < m_height; ++y)
        write_pixels(std::span<const uint16_t>(row, m_width));
    end_write();
}

void St7789::set_backlight(uint8_t brightness) {
    if (m_pin_backlight == 255) return;
    // Gamma-correct the 0-255 input onto the PWM counter's 0-65535 range --
    // matches the validated consumer driver's perceptual brightness curve.
    float gamma = 2.8f;
    uint16_t value = static_cast<uint16_t>(std::pow(static_cast<float>(brightness) / 255.0f, gamma) * 65535.0f + 0.5f);
    pwm_set_gpio_level(m_pin_backlight, value);
}

} // namespace pico_toolset

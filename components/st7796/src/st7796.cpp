#include "pico_toolset/st7796.h"

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#include <cmath>

namespace pico_toolset {

namespace {

// ST7796U command bytes used here, named and numbered after the ST7796S
// datasheet's own command sections (Sitronix, doc rev 2014/11 -- see
// docs/ST7796s.pdf in the PicoDoom repo this driver was consolidated
// against). Section numbers refer to that document.
constexpr uint8_t kCmdSwReset = 0x01;    // §9.2.1  SWRESET: Software Reset
constexpr uint8_t kCmdSlpOut = 0x11;     // §9.2.11 SLPOUT: Sleep Out
constexpr uint8_t kCmdInvOff = 0x20;     // §9.2.16 INVOFF: Display Inversion Off
constexpr uint8_t kCmdInvOn = 0x21;      // §9.2.17 INVON: Display Inversion On
constexpr uint8_t kCmdDispOn = 0x29;     // §9.2.19 DISPON: Display On
constexpr uint8_t kCmdCaset = 0x2A;      // §9.2.20 CASET: Column Address Set
constexpr uint8_t kCmdRaset = 0x2B;      // §9.2.21 RASET: Row Address Set
constexpr uint8_t kCmdRamwr = 0x2C;      // §9.2.22 RAMWR: Memory Write
constexpr uint8_t kCmdMadctl = 0x36;     // §9.2.28 MADCTL: Memory Data Access Control
constexpr uint8_t kCmdColmod = 0x3A;     // §9.2.32 COLMOD: Interface Pixel Format
constexpr uint8_t kCmdInvCtr = 0xB4;     // Display Inversion Control (column inversion)
constexpr uint8_t kCmdEntryMode = 0xB7;  // Entry Mode Set
constexpr uint8_t kCmdPwctr1 = 0xC0;     // Power Control 1
constexpr uint8_t kCmdPwctr2 = 0xC1;     // Power Control 2
constexpr uint8_t kCmdPwctr3 = 0xC2;     // Power Control 3
constexpr uint8_t kCmdVmctr1 = 0xC5;     // VCOM Control
constexpr uint8_t kCmdDoca = 0xE8;       // Display Output Ctrl Adjust
constexpr uint8_t kCmdGmctrp1 = 0xE0;    // Positive Gamma Control
constexpr uint8_t kCmdGmctrn1 = 0xE1;    // Negative Gamma Control
constexpr uint8_t kCmdCmdSet = 0xF0;     // Command Set Control (extended command set I/II enable/disable)

// COLMOD (3Ah) value: 16 bits/pixel on both the RGB and MCU interface
// nibbles (see St7796Config's doc comment on why this isn't a config knob).
constexpr uint8_t kColmod16Bit = 0x55;

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
    // Max drive strength + fast slew on the pins this bridge-style board
    // needs clean edges from (SCK/MOSI/DC/CS below -- MISO is an input, no
    // driver settings apply). RP2350 defaults to its weakest (2mA) setting;
    // this board's bridge-chip input trace needs stronger edges than that
    // to register reliably.
    gpio_set_drive_strength(config.pin_sck, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(config.pin_sck, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(config.pin_mosi, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(config.pin_mosi, GPIO_SLEW_RATE_FAST);

    gpio_init(m_pin_dc);
    gpio_set_dir(m_pin_dc, GPIO_OUT);
    gpio_set_drive_strength(m_pin_dc, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(m_pin_dc, GPIO_SLEW_RATE_FAST);

    gpio_init(m_pin_cs);
    gpio_set_dir(m_pin_cs, GPIO_OUT);
    gpio_set_drive_strength(m_pin_cs, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(m_pin_cs, GPIO_SLEW_RATE_FAST);
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
    // Ili9486's validated (real-hardware) reset sequence for the same board
    // family -- generous 500ms holds, not the datasheet-minimum 120ms this
    // file used before.
    if (config.pin_rst != 255) {
        gpio_put(config.pin_rst, true);
        sleep_ms(500);
        gpio_put(config.pin_rst, false);
        sleep_ms(500);
        gpio_put(config.pin_rst, true);
        sleep_ms(500);
    }

    write_command(kCmdSwReset);
    sleep_ms(120);
    write_command(kCmdSlpOut);
    sleep_ms(120);

    // Register sequence below is byte-for-byte the one SunFounder's own
    // Linux fbtft overlay sends to this exact panel (extracted from their
    // published mhs35ips-overlay.dtb's `init` property) -- prefer this
    // vendor-validated table over a datasheet/TFT_eSPI-derived one whenever
    // they disagree, since it's proven on the identical controller+panel
    // combination this driver targets.
    const uint8_t madctl[] = {config.madctl};
    write_command(kCmdMadctl, madctl, 1);

    const uint8_t colmod[] = {kColmod16Bit};
    write_command(kCmdColmod, colmod, 1);

    // Enable the extended command set (partI+partII) to reach the
    // power/gamma registers that follow; disabled again at the end.
    const uint8_t cmdset_en1[] = {0xC3};
    write_command(kCmdCmdSet, cmdset_en1, 1);
    const uint8_t cmdset_en2[] = {0x96};
    write_command(kCmdCmdSet, cmdset_en2, 1);

    const uint8_t invctr[] = {0x01}; // 1-dot inversion
    write_command(kCmdInvCtr, invctr, 1);

    const uint8_t entrymode[] = {0xC6};
    write_command(kCmdEntryMode, entrymode, 1);

    const uint8_t pwctr1[] = {0x80, 0x45};
    write_command(kCmdPwctr1, pwctr1, 2);
    const uint8_t pwctr2[] = {0x13};
    write_command(kCmdPwctr2, pwctr2, 1);
    const uint8_t pwctr3[] = {0xA7};
    write_command(kCmdPwctr3, pwctr3, 1);
    const uint8_t vmctr1[] = {0x0A};
    write_command(kCmdVmctr1, vmctr1, 1);

    const uint8_t doca[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
    write_command(kCmdDoca, doca, 8);

    const uint8_t gmctrp1[] = {0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30, 0x33,
                                0x47, 0x17, 0x13, 0x13, 0x2B, 0x31};
    write_command(kCmdGmctrp1, gmctrp1, 14);
    const uint8_t gmctrn1[] = {0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F, 0x33,
                                0x47, 0x38, 0x15, 0x16, 0x2C, 0x32};
    write_command(kCmdGmctrn1, gmctrn1, 14);

    const uint8_t cmdset_dis1[] = {0x3C};
    write_command(kCmdCmdSet, cmdset_dis1, 1);
    const uint8_t cmdset_dis2[] = {0x69};
    write_command(kCmdCmdSet, cmdset_dis2, 1);

    sleep_ms(120);

    // Panel-specific (see St7796Config::invert_colors's doc comment) --
    // send explicitly either way rather than relying on the reset default,
    // since that default isn't guaranteed consistent across ST7796U panel
    // batches/vendors.
    write_command(config.invert_colors ? kCmdInvOn : kCmdInvOff);
    write_command(kCmdDispOn);
    sleep_ms(100);

    if (m_pin_backlight != 255) {
        sleep_ms(50); // let the init sequence settle before lighting up
        set_backlight(255);
    }

    return true;
}

// Wire protocol: this board's LCD header sits behind a SPI-to-parallel
// bridge chip, like the ILI9486 panel this driver was originally validated
// next to -- confirmed by SunFounder's own published mhs35ips-overlay.dtb,
// which drives this exact board via Linux's fbtft "ilitek,ili9486" driver
// with `buswidth=8, regwidth=16`: every logical register value (the
// command byte, and separately each data/parameter byte) is padded to a
// 16-bit big-endian quantity (leading 0x00) over an 8-bit SPI bus -- see
// fbtft's fbtft_write_reg16_bus8()/define_fbtft_write_reg() (fbtft-bus.c
// upstream), which also asserts CS ONCE for the whole logical register
// write (command byte, then all its data bytes, D/C toggled low-then-high
// in between) rather than pulsing CS per individual byte -- hardware-
// confirmed 2026-09: real SunFounder ST7796U panel, this exact sequence.
// Bulk pixel data (after RAMWR) is NOT padded this way -- see
// set_window()/write_pixels() below, which switch to a continuous, unpadded
// CS-low burst once the bridge is in pixel-streaming mode.
void St7796::write_command(uint8_t cmd, const uint8_t* data, size_t len) {
    gpio_put(m_pin_cs, 0);

    gpio_put(m_pin_dc, 0);
    const uint8_t cmd_buf[2] = {0x00, cmd};
    spi_write_blocking(m_spi, cmd_buf, 2);

    if (data != nullptr && len > 0) {
        gpio_put(m_pin_dc, 1);
        for (size_t i = 0; i < len; ++i) {
            const uint8_t data_buf[2] = {0x00, data[i]};
            spi_write_blocking(m_spi, data_buf, 2);
        }
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

    write_command(kCmdRamwr); // own CS pulse, no data -- see write_command()'s doc comment

    // Pixel data is NOT byte-padded like commands/parameters are -- reopen
    // CS low here for a continuous, unpadded burst (write_pixels()/
    // end_write() below), matching Ili9486::set_window()'s identical
    // RAMWR-then-raw-burst transition.
    gpio_put(m_pin_dc, 1);
    gpio_put(m_pin_cs, 0);
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

#pragma once

#include <cstdint>
#include <span>

#include "hardware/spi.h"

namespace pico_toolset {

// Configuration for the ILI9486 SPI LCD (as used on Waveshare RP2350-PiZero
// style carrier boards, where the controller sits behind a 16-bit shift
// register). All pins and clocks are configurable.
//
// No field below has a working default -- pins/SPI instance/clocks are
// board wiring, not driver behavior, so this struct deliberately can't be
// default-constructed into something that just happens to work. Either
// start from a known-good preset in ili9486_configs.h (e.g.
// configs::ili9486::kWaveshareRp2350PiZero) or fill in every field yourself for a
// board this toolset doesn't have a preset for yet.
struct Ili9486Config {
    spi_inst_t* spi_instance = nullptr; // SPI peripheral -- must be set
    uint8_t     pin_sck;                // SPI SCK
    uint8_t     pin_mosi;               // SPI MOSI
    uint8_t     pin_miso;               // SPI MISO (touch controller shares the bus)
    uint8_t     pin_cs;                 // Chip Select
    uint8_t     pin_dc;                 // Data/Command
    uint8_t     pin_rst;                // Hardware Reset
    uint8_t     pin_backlight = 255;    // Backlight PWM pin (255 = none)
    uint32_t    spi_freq_hz  = 8000000;   // Command/parameter clock
    uint32_t    pixel_freq_hz = 25000000; // Pixel-streaming clock
    bool        use_dma      = true;   // DMA-backed pixel streaming when available
    // Which DMA channel write_pixels() uses when use_dma is true. -1 (the
    // default) auto-claims any free channel via dma_claim_unused_channel(),
    // same as this driver has always done. Set to a specific channel (0-11)
    // to take control over channel assignment instead -- e.g. a consumer
    // whose own code elsewhere claims a *specific* DMA channel by hardcoded
    // number before this driver initializes (some vendored libraries do,
    // rather than auto-detecting) needs a way to keep this driver off of it,
    // since "first free channel" can otherwise collide with that hardcoded
    // assumption depending on init order. This driver claims the channel
    // itself either way (via dma_channel_claim() in the explicit case) --
    // callers should not also claim `dma_channel` themselves.
    int         dma_channel  = -1;
};

// Driver for the ILI9486 480x320 RGB565 TFT LCD over SPI.
//
// WIRE PROTOCOL (Waveshare 3.5" RPi LCD family): the controller is behind a
// 16-bit shift register, so
//   1. a command byte is sent with D/C low;
//   2. a register parameter is SIXTEEN bits (0x00 pad + value) with D/C high;
//   3. CS is pulsed around EVERY individual command/parameter; pixel data is
//      the one exception -- it streams as one continuous CS-low burst.
//
// Derived from TOM6809's validated driver + PicoDoom's DMA enhancement (MIT).
class Ili9486 {
public:
    static constexpr int kWidth  = 480;
    static constexpr int kHeight = 320;

    // Resets the panel and runs the ILI9486 init register sequence.
    bool init(const Ili9486Config& config);

    // Opens the RAMWR window (CASET/PASET, inclusive). CS stays asserted;
    // callers MUST call end_write() once done. No other SPI user (e.g. a
    // touch controller sharing the bus) may transfer in between.
    void set_window(int x0, int y0, int x1, int y1);

    // Streams RGB565 pixels (already big-endian byte order on the wire) into
    // the window opened by set_window(). Callers must not overrun the window.
    void write_pixels(std::span<const uint16_t> pixels);

    // Deasserts CS after a set_window()/write_pixels() sequence.
    void end_write();

    // Fills the whole panel with one solid color.
    void fill_solid(uint16_t rgb565);

    // PWM backlight control (0-255). No-op if pin_backlight is 255.
    void set_backlight(uint8_t brightness);

    // Accessor for sharing the SPI bus (e.g. with an XPT2046 touch).
    [[nodiscard]] spi_inst_t* spi() const { return m_spi; }

private:
    void claim_bus();
    void write_command(uint8_t cmd);
    void write_data_byte(uint8_t data);

    spi_inst_t* m_spi = spi1;
    uint8_t     m_pin_cs = 8;
    uint8_t     m_pin_dc = 24;
    uint8_t     m_pin_rst = 25;
    uint8_t     m_pin_backlight = 255;
    uint32_t    m_command_baud = 8'000'000;
    uint32_t    m_pixel_baud = 25'000'000;
    bool        m_use_dma = true;
    int         m_dma_chan = -1;
};

} // namespace pico_toolset
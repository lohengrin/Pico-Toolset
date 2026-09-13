#pragma once

#include <cstdint>
#include <span>

#include "hardware/spi.h"

#include "pico_toolset/display_panel.h"

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
// Derived from a validated driver + DMA enhancement (MIT).
//
// Implements DisplayPanel (display_panel.h) -- the low-level windowed/DMA
// streaming contract it shares with St7789 -- so callers that only need
// that contract (not a framebuffer) can hold a DisplayPanel& instead of a
// concrete Ili9486& and swap panels without changing call sites.
class Ili9486 : public DisplayPanel {
public:
    static constexpr int kWidth  = 480;
    static constexpr int kHeight = 320;

    // Resets the panel and runs the ILI9486 init register sequence.
    bool init(const Ili9486Config& config);

    [[nodiscard]] int width() const override { return kWidth; }
    [[nodiscard]] int height() const override { return kHeight; }

    // Opens the RAMWR window (CASET/PASET, inclusive). CS stays asserted;
    // callers MUST call end_write() once done. No other SPI user (e.g. a
    // touch controller sharing the bus) may transfer in between.
    void set_window(int x0, int y0, int x1, int y1) override;

    // Streams RGB565 pixels (already big-endian byte order on the wire) into
    // the window opened by set_window(). Callers must not overrun the window.
    // Blocks until the whole transfer (and its SPI-level cleanup) is done --
    // see start_pixels_dma() below for a non-blocking alternative.
    void write_pixels(std::span<const uint16_t> pixels) override;

    // Non-blocking pixel streaming, for callers that need to do other work
    // (e.g. a USB host stack's task()) while a large transfer is in flight
    // instead of blocking the CPU for its whole duration. Usage:
    //   start_pixels_dma(pixels);
    //   while (pixels_busy()) { other_work(); }
    //   finish_pixels_dma();
    // before the next set_window()/write_pixels()/end_write(). Falls back to
    // a synchronous transfer when DMA isn't available/configured (use_dma
    // false, or too high a pixel_freq_hz) -- pixels_busy() then always
    // reports false and finish_pixels_dma() is a no-op, so callers can use
    // this trio unconditionally regardless of config.
    void start_pixels_dma(std::span<const uint16_t> pixels) override;
    [[nodiscard]] bool pixels_busy() const override;
    void finish_pixels_dma() override;

    // Deasserts CS after a set_window()/write_pixels() (or
    // start_pixels_dma()/finish_pixels_dma()) sequence.
    void end_write() override;

    // Fills the whole panel with one solid color.
    void fill_solid(uint16_t rgb565) override;

    // PWM backlight control (0-255). No-op if pin_backlight is 255.
    void set_backlight(uint8_t brightness) override;

    // Accessor for sharing the SPI bus (e.g. with an XPT2046 touch).
    [[nodiscard]] spi_inst_t* spi() const override { return m_spi; }

    // Live pixel-clock tuning, for finding a panel/wiring's real corruption
    // ceiling on actual hardware instead of guessing from a datasheet.
    // set_pixel_clock_hz() just updates the requested rate for future
    // set_window() calls (which re-applies it via spi_set_baudrate() every
    // time already, so no re-init is needed -- takes effect next frame).
    // Plain field writes, not synchronized: fine for a caller on a different
    // core than the one calling set_window() (e.g. tuning from a keypress
    // handler while a blit loop runs on another core) since the worst case
    // is one frame using a stale rate, not corruption of the field itself.
    void set_pixel_clock_hz(uint32_t hz) { m_pixel_baud = hz; }
    [[nodiscard]] uint32_t pixel_clock_hz() const { return m_pixel_baud; }
    // What spi_set_baudrate() actually achieved as of the last set_window()
    // call (its return value) -- SPI's integer clock divider means an
    // arbitrary requested hz gets rounded, so this can differ from
    // pixel_clock_hz(). 0 before the first set_window() call.
    [[nodiscard]] uint32_t pixel_clock_actual_hz() const { return m_pixel_baud_actual; }

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
    uint32_t    m_pixel_baud_actual = 0;
    bool        m_use_dma = true;
    int         m_dma_chan = -1;
};

} // namespace pico_toolset
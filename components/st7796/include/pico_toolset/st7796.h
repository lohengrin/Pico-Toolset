#pragma once

#include <cstdint>
#include <span>

#include "hardware/spi.h"

#include "pico_toolset/display_panel.h"

namespace pico_toolset {

// Configuration for the ST7796U SPI TFT LCD (as used on Waveshare
// RP2350-PiZero style carrier boards, replacing an ILI9486 panel wired the
// same way -- same MISO-sharing-with-touch requirement, same reset/backlight
// pin roles, unlike ILI9486's 16-bit-shift-register wire protocol). All
// pins and the clock are configurable.
//
// No field below has a working default -- pins/SPI instance/clock/geometry
// are board+panel wiring, not driver behavior, so this struct deliberately
// can't be default-constructed into something that just happens to work.
// Either start from a known-good preset in st7796_configs.h (e.g.
// configs::st7796::kWaveshareRp2350PiZero) or fill in every field yourself
// for a board this toolset doesn't have a preset for yet.
struct St7796Config {
    spi_inst_t* spi_instance = nullptr; // SPI peripheral -- must be set
    uint8_t     pin_sck;                // SPI SCK
    uint8_t     pin_mosi;               // SPI MOSI
    uint8_t     pin_miso;               // SPI MISO -- shared with a touch controller on the same bus
    uint8_t     pin_cs;                 // Chip Select
    uint8_t     pin_dc;                 // Data/Command
    uint8_t     pin_rst = 255;          // Hardware reset pin (255 = none -- rely on the panel's software SWRESET only)
    uint8_t     pin_backlight = 255;    // Backlight PWM pin (255 = none)

    uint16_t width;                     // Panel width in the wired rotation
    uint16_t height;                    // Panel height in the wired rotation
    uint16_t col_offset = 0;            // CASET start offset (panel RAM can be larger than the visible area)
    uint16_t row_offset = 0;            // RASET start offset
    uint8_t  madctl = 0x00;             // Orientation/color-order register value -- panel+rotation specific

    // One SPI clock for both commands and pixel data (ST7796U is a direct
    // SPI-wired panel, not behind ILI9486's shift-register bridge, so there
    // is no separate slow "command clock" concern here). Live-tunable at
    // runtime via St7796::set_pixel_clock_hz() once the panel's real
    // corruption ceiling on actual hardware is known -- start conservative
    // (e.g. reuse whatever rate an existing ILI9486 board on the same
    // wiring was already validated at) and raise it from there, the same
    // way Ili9486Config::pixel_freq_hz was tuned.
    uint32_t spi_freq_hz = 8'000'000;
    bool     use_dma = true;            // DMA-backed pixel streaming when available
    // Which DMA channel write_pixels() uses when use_dma is true. -1 (the
    // default) auto-claims any free channel via dma_claim_unused_channel() --
    // see Ili9486Config::dma_channel's doc comment for the full rationale on
    // when to pin a specific channel instead.
    int      dma_channel = -1;
};

// Driver for the ST7796U SPI TFT LCD (RGB565), landscape-oriented boards
// like the Waveshare 3.5" panel this was written against.
//
// WIRE PROTOCOL: a command byte is sent with D/C low, CS pulsed low around
// it; any parameter/pixel data that follows is sent with D/C high in the
// same CS-low span. Pixel data streams as one continuous CS-low burst.
// CASET/RASET are each a single 4-byte burst (not ILI9486's
// one-byte-at-a-time-with-0x00-padding shift-register scheme) -- same wire
// shape as St7789, which this driver's structure mirrors closely (ST7796U
// is a Sitronix-family controller like ST7789, sharing MADCTL/COLMOD/
// CASET/RASET/RAMWR command codes).
//
// Implements DisplayPanel (display_panel.h) -- the low-level windowed/DMA
// streaming contract it shares with Ili9486/St7789 -- so callers that only
// need that contract (not a framebuffer) can hold a DisplayPanel& instead
// of a concrete St7796& and swap panels without changing call sites.
class St7796 : public DisplayPanel {
public:
    // Resets the panel and runs the ST7796U init register sequence using
    // the geometry/MADCTL values from `config`.
    bool init(const St7796Config& config);

    [[nodiscard]] int width() const override { return m_width; }
    [[nodiscard]] int height() const override { return m_height; }

    // Opens the RAMWR window (CASET/RASET, inclusive). CS stays asserted;
    // callers MUST call end_write() once done. No other SPI user (e.g. a
    // touch controller sharing the bus) may transfer in between.
    void set_window(int x0, int y0, int x1, int y1) override;

    // Streams RGB565 pixels (already big-endian byte order on the wire)
    // into the window opened by set_window(). Callers must not overrun the
    // window. Blocks until the whole transfer (and its SPI-level cleanup)
    // is done -- see start_pixels_dma() below for a non-blocking alternative.
    void write_pixels(std::span<const uint16_t> pixels) override;

    // Non-blocking pixel streaming, for callers that need to do other work
    // while a large transfer is in flight instead of blocking the CPU for
    // its whole duration. Usage:
    //   start_pixels_dma(pixels);
    //   while (pixels_busy()) { other_work(); }
    //   finish_pixels_dma();
    // before the next set_window()/write_pixels()/end_write(). Falls back to
    // a synchronous transfer when DMA isn't available/configured -- see
    // Ili9486's identically-shaped trio for the exact same fallback contract.
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

    // Accessor for sharing the SPI bus (e.g. with an XPT2046 touch
    // controller, as on the Waveshare RP2350-PiZero + 3.5" panel).
    [[nodiscard]] spi_inst_t* spi() const override { return m_spi; }

    // Live SPI-clock tuning, for finding a panel/wiring's real corruption
    // ceiling on actual hardware instead of guessing from a datasheet --
    // same shape as Ili9486::set_pixel_clock_hz()/pixel_clock_hz()/
    // pixel_clock_actual_hz(), so PicoDoom-style callers that already tune
    // an ILI9486 panel this way can switch to St7796 with the same calling
    // pattern. Just updates the requested rate for future set_window()
    // calls (which re-applies it via spi_set_baudrate() every time already,
    // so no re-init is needed -- takes effect next frame). Plain field
    // writes, not synchronized: fine for a caller on a different core than
    // the one calling set_window(), since the worst case is one frame using
    // a stale rate, not corruption of the field itself.
    void set_pixel_clock_hz(uint32_t hz) { m_spi_freq_hz = hz; }
    [[nodiscard]] uint32_t pixel_clock_hz() const { return m_spi_freq_hz; }
    // What spi_set_baudrate() actually achieved as of the last set_window()
    // call (its return value) -- SPI's integer clock divider means an
    // arbitrary requested hz gets rounded, so this can differ from
    // pixel_clock_hz(). 0 before the first set_window() call.
    [[nodiscard]] uint32_t pixel_clock_actual_hz() const { return m_spi_freq_actual_hz; }

private:
    void write_command(uint8_t cmd, const uint8_t* data = nullptr, size_t len = 0);

    spi_inst_t* m_spi = spi1;
    uint8_t     m_pin_cs = 0;
    uint8_t     m_pin_dc = 0;
    uint8_t     m_pin_backlight = 255;
    uint16_t    m_width = 0;
    uint16_t    m_height = 0;
    uint16_t    m_col_offset = 0;
    uint16_t    m_row_offset = 0;
    uint32_t    m_spi_freq_hz = 8'000'000;
    uint32_t    m_spi_freq_actual_hz = 0;
    bool        m_use_dma = true;
    int         m_dma_chan = -1;
};

} // namespace pico_toolset

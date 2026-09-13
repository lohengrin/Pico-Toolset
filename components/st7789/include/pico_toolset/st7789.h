#pragma once

#include <cstdint>
#include <span>

#include "hardware/spi.h"

#include "pico_toolset/display_panel.h"

namespace pico_toolset {

// Configuration for an ST7789 SPI TFT LCD. ST7789 ships in several
// panel sizes/orientations (240x240, 240x135, 320x240, ...), each needing
// its own CASET/RASET RAM offset and MADCTL (orientation/color-order)
// value -- rather than switching on width/height internally (as the
// consumer driver this was extracted from did), those are plain config
// fields: see st7789_configs.h for known-good per-board presets (e.g.
// configs::st7789::kElecrowCrowPanelPicoHmi28), or fill in every field
// yourself for a panel this toolset doesn't have a preset for yet.
//
// No field below has a working default -- pins/SPI instance/panel
// geometry/MADCTL are board+panel wiring, not driver behavior.
struct St7789Config {
    spi_inst_t* spi_instance = nullptr; // SPI peripheral -- must be set
    uint8_t     pin_sck;                // SPI SCK
    uint8_t     pin_mosi;               // SPI MOSI
    uint8_t     pin_cs;                 // Chip Select
    uint8_t     pin_dc;                 // Data/Command
    uint8_t     pin_reset = 255;        // Hardware reset pin (255 = none -- rely on the panel's software SWRESET only)
    uint8_t     pin_backlight = 255;    // Backlight PWM pin (255 = none)

    uint16_t width;                     // Panel width in the wired rotation
    uint16_t height;                    // Panel height in the wired rotation
    uint16_t col_offset = 0;            // CASET start offset (panel RAM is often larger than the visible area)
    uint16_t row_offset = 0;            // RASET start offset
    uint8_t  madctl = 0x00;             // Orientation/color-order register value -- panel+rotation specific
    bool     tearing_effect_on = false; // TEON vs TEOFF (frame-sync signal, unused by most consumers)
    bool     inversion_on = false;      // INVON vs INVOFF -- some panels need color inversion

    uint32_t spi_freq_hz = 62'500'000;  // Single SPI clock for commands and pixel data alike
    bool     use_dma = true;            // DMA-backed pixel streaming when available
    // Which DMA channel write_pixels() uses when use_dma is true. -1 (the
    // default) auto-claims any free channel via dma_claim_unused_channel().
    // Set to a specific channel (0-11) if a consumer's other code elsewhere
    // claims a specific channel by hardcoded number before this driver
    // initializes -- see pico_toolset_ili9486's Ili9486Config::dma_channel
    // doc comment for the full rationale.
    int      dma_channel = -1;
};

// Driver for ST7789-family SPI TFT LCDs (RGB565).
//
// WIRE PROTOCOL: a command byte is sent with D/C low, CS pulsed low around
// it; any parameter/pixel data that follows is sent with D/C high in the
// same CS-low span. Pixel data streams as one continuous CS-low burst
// (matching Ili9486's shape, but ST7789's CASET/RASET are each a single
// 4-byte burst rather than one-byte-at-a-time-with-padding).
//
// Derived from a real-hardware-validated driver (itself a fork of
// Pimoroni's MIT-licensed ST7789 driver, extended for a non-Pimoroni panel
// variant) -- see AGENTS.md's "Source lineage" for the full attribution.
// Implements DisplayPanel (display_panel.h) -- the low-level windowed/DMA
// streaming contract it shares with Ili9486 -- so callers that only need
// that contract (not a framebuffer) can hold a DisplayPanel& instead of a
// concrete St7789& and swap panels without changing call sites.
class St7789 : public DisplayPanel {
public:
    // Resets the panel and runs the ST7789 init register sequence using the
    // geometry/MADCTL/inversion/tearing-effect values from `config`.
    bool init(const St7789Config& config);

    // Opens the RAMWR window (CASET/RASET, inclusive). CS stays asserted;
    // callers MUST call end_write() once done. No other SPI user (e.g. a
    // touch controller or SD card sharing the bus) may transfer in between.
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

    // Accessor for sharing the SPI bus (e.g. with an XPT2046 touch or an SD
    // card, as on the CrowPanel PICO HMI 2.8").
    [[nodiscard]] spi_inst_t* spi() const override { return m_spi; }

    [[nodiscard]] int width() const override { return m_width; }
    [[nodiscard]] int height() const override { return m_height; }

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
    uint32_t    m_spi_freq_hz = 62'500'000;
    bool        m_use_dma = true;
    int         m_dma_chan = -1;
};

} // namespace pico_toolset

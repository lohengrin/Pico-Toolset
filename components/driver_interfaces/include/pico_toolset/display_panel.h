#pragma once

#include <cstdint>
#include <span>

#include "hardware/spi.h"

namespace pico_toolset {

// Low-level contract shared by windowed/DMA-streamed RGB565 SPI panels
// (e.g. Ili9486, St7789). NOT for framebuffer-model displays like Ssd1306
// (I2C, 1bpp, no windowed/DMA concept) -- see libs/screen's DisplayDriver
// for the higher, chip-agnostic framebuffer/widget layer those use instead.
//
// A concrete panel's init(Config&) is deliberately NOT part of this
// interface -- config types are driver-specific by design (see this
// toolset's "Config struct for everything" convention), so callers always
// construct and initialize the concrete class first, then use it as a
// DisplayPanel& from there on.
class DisplayPanel {
public:
    virtual ~DisplayPanel() = default;

    [[nodiscard]] virtual int width() const = 0;
    [[nodiscard]] virtual int height() const = 0;

    // Opens the RAMWR window (inclusive). CS stays asserted; callers MUST
    // call end_write() once done. No other bus user (e.g. a touch
    // controller sharing the SPI bus) may transfer in between.
    virtual void set_window(int x0, int y0, int x1, int y1) = 0;

    // Streams RGB565 pixels (big-endian on the wire) into the window opened
    // by set_window(). Blocks until the whole transfer is done -- see
    // start_pixels_dma() for a non-blocking alternative.
    virtual void write_pixels(std::span<const uint16_t> pixels) = 0;

    // Non-blocking pixel streaming trio, for callers that need to do other
    // work while a large transfer is in flight (e.g. a USB host stack's
    // task()) instead of blocking the CPU for its whole duration:
    //   start_pixels_dma(pixels);
    //   while (pixels_busy()) { other_work(); }
    //   finish_pixels_dma();
    // Default implementation falls back to a synchronous write_pixels() +
    // an always-false/no-op busy/finish pair, so a panel without DMA
    // support only needs to implement write_pixels() -- callers can use
    // this trio unconditionally regardless of whether the concrete panel
    // has a real async path.
    virtual void start_pixels_dma(std::span<const uint16_t> pixels) { write_pixels(pixels); }
    [[nodiscard]] virtual bool pixels_busy() const { return false; }
    virtual void finish_pixels_dma() {}

    // Deasserts CS after a set_window()/write_pixels() (or
    // start_pixels_dma()/finish_pixels_dma()) sequence. Default no-op for a
    // panel with no such bus-release step.
    virtual void end_write() {}

    // Fills the whole panel with one solid color.
    virtual void fill_solid(uint16_t rgb565) = 0;

    // Optional: set panel backlight (0-255). Default no-op.
    virtual void set_backlight(uint8_t /*brightness*/) {}

    // Bus-sharing accessor for a co-located touch controller (or other
    // device on the same SPI bus). Default nullptr for panels not on a
    // shared/nameable bus.
    [[nodiscard]] virtual spi_inst_t* spi() const { return nullptr; }
};

} // namespace pico_toolset

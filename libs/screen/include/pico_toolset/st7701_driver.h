#pragma once

#if PICO_RP2350

#include <algorithm>
#include <cstdint>

#include "pico_toolset/display_driver.h"
#include "pico_toolset/st7701.h"

namespace pico_toolset {

// DisplayDriver over a scanned-out ST7701 panel. Draws directly into the
// panel's own framebuffer (the one passed in St7701Config::framebuffer): there
// is nothing to upload, so flush() only waits for the next vertical blank.
// Draw between flushes if you want to avoid mid-frame partial updates, or draw
// into your own buffer and call St7701::present() instead for tear-free full
// frames.
//
// Formats: Rgb565 stores st7701_encode()d pixels; Palette8 stores RGB332
// indices (the panel's default colour table -- do not repurpose the table
// while using this adaptor); Scanline565 has no framebuffer, so drawing calls
// are ignored (render from the line callback instead).
class St7701Driver : public DisplayDriver {
public:
    explicit St7701Driver(St7701& panel) : m_panel(panel), m_format(panel.format()) {}

    [[nodiscard]] int width() const override { return m_panel.width(); }
    [[nodiscard]] int height() const override { return m_panel.height(); }

    void set_pixel(int x, int y, Color color) override {
        if (x < 0 || y < 0 || x >= width() || y >= height()) return;
        const size_t i = static_cast<size_t>(y) * width() + x;
        if (m_format == St7701Format::Rgb565) m_panel.framebuffer()[i] = st7701_encode(color.rgb565);
        else if (m_format == St7701Format::Palette8) m_panel.framebuffer8()[i] = rgb332(color);
    }

    void fill_rect(int x0, int y0, int x1, int y1, Color color) override {
        x0 = std::max(0, x0);
        y0 = std::max(0, y0);
        x1 = std::min(width() - 1, x1);
        y1 = std::min(height() - 1, y1);
        if (x1 < x0) return;
        for (int y = y0; y <= y1; ++y) {
            const size_t row = static_cast<size_t>(y) * width();
            if (m_format == St7701Format::Rgb565) {
                uint16_t* p = m_panel.framebuffer() + row;
                std::fill(p + x0, p + x1 + 1, st7701_encode(color.rgb565));
            } else if (m_format == St7701Format::Palette8) {
                uint8_t* p = m_panel.framebuffer8() + row;
                std::fill(p + x0, p + x1 + 1, rgb332(color));
            }
        }
    }

    void flush() override { m_panel.wait_for_vsync(); }

    void set_backlight(uint8_t level) override { m_panel.set_backlight(level); }

private:
    static uint8_t rgb332(Color c) {
        const uint16_t v = c.rgb565;
        return static_cast<uint8_t>((((v >> 13) & 7) << 5) | (((v >> 8) & 7) << 2) | ((v >> 3) & 3));
    }

    St7701& m_panel;
    St7701Format m_format;
};

} // namespace pico_toolset

#endif // PICO_RP2350

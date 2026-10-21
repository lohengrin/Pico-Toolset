#pragma once

#if PICO_RP2350

#include <algorithm>
#include <cstdint>

#include "pico_toolset/display_driver.h"
#include "pico_toolset/st7701.h"

namespace pico_toolset {

// DisplayDriver over a scanned-out ST7701 panel. Draws directly into the
// panel's own framebuffer (the one passed in St7701Config::framebuffer, in
// st7701_encode()d byte order): there is nothing to upload, so flush() only
// waits for the next vertical blank. Draw between flushes if you want to
// avoid mid-frame partial updates, or draw into your own buffer and call
// St7701::present() instead for tear-free full frames.
class St7701Driver : public DisplayDriver {
public:
    explicit St7701Driver(St7701& panel) : m_panel(panel) {}

    [[nodiscard]] int width() const override { return m_panel.width(); }
    [[nodiscard]] int height() const override { return m_panel.height(); }

    void set_pixel(int x, int y, Color color) override {
        if (x < 0 || y < 0 || x >= width() || y >= height()) return;
        m_panel.framebuffer()[static_cast<size_t>(y) * width() + x] = st7701_encode(color.rgb565);
    }

    void fill_rect(int x0, int y0, int x1, int y1, Color color) override {
        x0 = std::max(0, x0);
        y0 = std::max(0, y0);
        x1 = std::min(width() - 1, x1);
        y1 = std::min(height() - 1, y1);
        const uint16_t v = st7701_encode(color.rgb565);
        for (int y = y0; y <= y1; ++y) {
            uint16_t* row = m_panel.framebuffer() + static_cast<size_t>(y) * width();
            std::fill(row + x0, row + x1 + 1, v);
        }
    }

    void flush() override { m_panel.wait_for_vsync(); }

    void set_backlight(uint8_t level) override { m_panel.set_backlight(level); }

private:
    St7701& m_panel;
};

} // namespace pico_toolset

#endif // PICO_RP2350

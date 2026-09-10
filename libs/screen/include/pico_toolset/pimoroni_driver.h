// Optional Pimoroni PicoGraphics backend. Only compiled when
// PICO_TOOLSET_SCREEN_PIMORONI is ON; requires pimoroni-pico's pico_graphics
// library to be on the include/link path (see README).
#if defined(PICO_TOOLSET_SCREEN_PIMORONI)

#pragma once

#include <cstdint>

#include "libraries/pico_graphics/pico_graphics.hpp"

#include "pico_toolset/display_driver.h"

namespace pico_toolset {

// DisplayDriver adaptor over a pimoroni::PicoGraphics instance configured for
// RGB565 (or another 16-bit-per-pixel format). flush() is a no-op: PicoGraphics
// backs straight into its own framebuffer, which its display class then owns.
class PimoroniDriver : public DisplayDriver {
public:
    explicit PimoroniDriver(pimoroni::PicoGraphics& graphics) : m_graphics(graphics) {}

    int width() const override { return m_graphics.bounds.w; }
    int height() const override { return m_graphics.bounds.h; }

    void set_pixel(int x, int y, Color color) override {
        if (x < 0 || y < 0 || x >= width() || y >= height()) return;
        // PicoGraphics uses the pen type matching its declared pixel format.
        m_graphics.set_pen(color.rgb565);
        m_graphics.pixel(pimoroni::Point(x, y));
    }

    void fill_rect(int x0, int y0, int x1, int y1, Color color) override {
        m_graphics.set_pen(color.rgb565);
        m_graphics.rectangle(pimoroni::Rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1));
    }

    // flush() intentionally a no-op -- PicoGraphics targets the display
    // directly; the owning display class handles presentation.
    void flush() override {}

private:
    pimoroni::PicoGraphics& m_graphics;
};

} // namespace pico_toolset

#endif // PICO_TOOLSET_SCREEN_PIMORONI
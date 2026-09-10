#pragma once

#include <cstdint>

#include "pico_toolset/display_driver.h"
#include "pico_toolset/ssd1306.h"

namespace pico_toolset {

// DisplayDriver adaptor over pico_toolset::Ssd1306. The SSD1306 is
// monochrome -- any color whose RGB565 != 0 draws the pixel on.
class Ssd1306Driver : public DisplayDriver {
public:
    explicit Ssd1306Driver(Ssd1306& display) : m_display(display) {}

    int width() const override { return m_display.width(); }
    int height() const override { return m_display.height(); }

    void set_pixel(int x, int y, Color color) override {
        if (color.rgb565 != 0) {
            m_display.draw_pixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
        } else {
            uint32_t ux = static_cast<uint32_t>(x), uy = static_cast<uint32_t>(y);
            uint8_t* buf = m_display.buffer();
            if (ux < m_display.width() && uy < m_display.height() && buf)
                buf[ux + m_display.width() * (uy >> 3)] &= static_cast<uint8_t>(~(0x1u << (uy & 7)));
        }
    }

    void fill_rect(int x0, int y0, int x1, int y1, Color color) override {
        const int w = m_display.width();
        const int h = m_display.height();
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                if (x < 0 || y < 0 || x >= w || y >= h) continue;
                set_pixel(x, y, color);
            }
    }

    void flush() override { m_display.show(); }

private:
    Ssd1306& m_display;
};

} // namespace pico_toolset
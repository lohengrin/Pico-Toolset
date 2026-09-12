#pragma once

#include <algorithm>
#include <cstdint>

#include "pico_toolset/display_driver.h"
#include "pico_toolset/st7789.h"

namespace pico_toolset {

// DisplayDriver adaptor over pico_toolset::St7789 using a caller-owned
// RGB565 framebuffer (e.g. 320x240x2 = 153600 bytes). The framebuffer is NOT
// owned or freed by this class. Same dirty-rect + flush() shape as
// Ili9486Driver -- see that file if you need the reasoning behind it.
class St7789Driver : public DisplayDriver {
public:
    St7789Driver(St7789& panel, uint16_t* framebuffer)
        : m_panel(panel), m_fb(framebuffer) {}

    int width() const override { return m_panel.width(); }
    int height() const override { return m_panel.height(); }

    void set_pixel(int x, int y, Color color) override {
        if (x < 0 || y < 0 || x >= width() || y >= height()) return;
        size_t i = static_cast<size_t>(y) * width() + static_cast<size_t>(x);
        m_fb[i] = color.rgb565;
        mark_dirty(x, y);
    }

    void fill_rect(int x0, int y0, int x1, int y1, Color color) override {
        x0 = std::max(0, x0);
        y0 = std::max(0, y0);
        x1 = std::min(width() - 1, x1);
        y1 = std::min(height() - 1, y1);
        for (int y = y0; y <= y1; ++y) {
            size_t base = static_cast<size_t>(y) * width();
            for (int x = x0; x <= x1; ++x)
                m_fb[base + x] = color.rgb565;
        }
        mark_dirty(x0, y0);
        mark_dirty(x1, y1);
    }

    // Upload the dirty rectangle (rows only, x-range widened to [minX..maxX])
    // to the panel.
    void flush() override {
        if (!m_dirty) return;
        if (m_min_x > m_max_x || m_min_y > m_max_y) { m_dirty = false; return; }
        const int linelen = m_max_x - m_min_x + 1;
        m_panel.set_window(m_min_x, m_min_y, m_max_x, m_max_y);
        for (int y = m_min_y; y <= m_max_y; ++y) {
            size_t base = static_cast<size_t>(y) * width() + m_min_x;
            for (int i = 0; i < linelen; ++i) {
                uint16_t v = m_fb[base + i];
                m_line[i] = static_cast<uint16_t>((v << 8) | (v >> 8)); // big-endian on the wire
            }
            m_panel.write_pixels({m_line, static_cast<size_t>(linelen)});
        }
        m_panel.end_write();
        m_dirty = false;
    }

    void set_backlight(uint8_t level) override { m_panel.set_backlight(level); }

    uint16_t* framebuffer() const { return m_fb; }
    bool dirty() const { return m_dirty; }

private:
    void mark_dirty(int x, int y) {
        if (!m_dirty) {
            m_min_x = m_max_x = x;
            m_min_y = m_max_y = y;
            m_dirty = true;
        } else {
            m_min_x = std::min(m_min_x, x);
            m_max_x = std::max(m_max_x, x);
            m_min_y = std::min(m_min_y, y);
            m_max_y = std::max(m_max_y, y);
        }
    }

    St7789& m_panel;
    uint16_t* m_fb = nullptr;
    uint16_t m_line[320]{}; // BE byte-swap staging, sized for the widest validated panel
    int m_min_x = 0, m_min_y = 0, m_max_x = -1, m_max_y = -1;
    bool m_dirty = false;
};

} // namespace pico_toolset

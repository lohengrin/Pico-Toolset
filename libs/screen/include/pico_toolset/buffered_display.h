#pragma once

#include <algorithm>
#include <cstdint>

#include "pico_toolset/display_driver.h"
#include "pico_toolset/display_panel.h"

namespace pico_toolset {

// DisplayDriver adaptor over any DisplayPanel (e.g. Ili9486, St7789) using a
// caller-owned RGB565 framebuffer (e.g. allocated from PSRAM for a
// 480x320 panel: 480*320*2 = 307200 bytes). The framebuffer is NOT owned or
// freed by this class, and `panel` must outlive it.
//
// set_pixel()/fill_rect() write into the framebuffer (native byte order)
// and track the dirtied region; flush() uploads only the dirty rows back to
// the panel via its start_pixels_dma()/pixels_busy()/finish_pixels_dma()
// trio (a per-row spin-wait, not a yield-to-caller async flush -- but this
// still goes through the panel's real DMA path where one exists, and falls
// back to a synchronous write_pixels() automatically where it doesn't, per
// DisplayPanel's own default, so this works unconditionally). Replaces the
// old, per-chip Ili9486Driver/St7789Driver -- same dirty-rect logic,
// generalized to work against any current or future DisplayPanel
// implementer.
class BufferedDisplay : public DisplayDriver {
public:
    // `line_buffer_capacity` must be at least the panel's width -- it sizes
    // the internal big-endian byte-swap staging row. Defaults to `panel`'s
    // current width().
    BufferedDisplay(DisplayPanel& panel, uint16_t* framebuffer)
        : m_panel(panel), m_fb(framebuffer) {}

    [[nodiscard]] int width() const override { return m_panel.width(); }
    [[nodiscard]] int height() const override { return m_panel.height(); }

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
    // to the panel, non-blocking-DMA-driven where the wrapped panel
    // supports it.
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
            m_panel.start_pixels_dma({m_line, static_cast<size_t>(linelen)});
            while (m_panel.pixels_busy()) { /* spin -- see DisplayPanel::pixels_busy() */ }
            m_panel.finish_pixels_dma();
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

    DisplayPanel& m_panel;
    uint16_t* m_fb = nullptr;
    // Sized for the widest panel this toolset currently ships (Ili9486's
    // 480px) -- a future wider panel needs this bumped alongside it.
    uint16_t m_line[480]{}; // BE byte-swap staging
    int m_min_x = 0, m_min_y = 0, m_max_x = -1, m_max_y = -1;
    bool m_dirty = false;
};

} // namespace pico_toolset

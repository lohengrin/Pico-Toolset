#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>

#include "pico_toolset/display_driver.h"

namespace pico_toolset {

// Bitmap text glyph. `data` is a width x height bitmask, one bit = one pixel,
// MSB = leftmost, rows packed little-endian.
struct BitmapGlyph {
    uint8_t width;
    uint8_t height;
    const uint8_t* data;
};

// Base class for anything that can draw itself onto a DisplayDriver.
class Widget {
public:
    Widget() = default;
    virtual ~Widget() = default;

    // Widgets are drawn inside their slot's rectangle; nothing outside their
    // bounding box is reported dirty.
    virtual void draw(DisplayDriver& display) const = 0;
};

// Solid-color rectangle widget (backgrounds, simple bars).
class RectWidget : public Widget {
public:
    RectWidget(int x, int y, int w, int h, Color color)
        : m_x(x), m_y(y), m_w(w), m_h(h), m_color(color) {}

    void draw(DisplayDriver& display) const override {
        display.fill_rect(m_x, m_y, m_x + m_w - 1, m_y + m_h - 1, m_color);
    }

    int x() const { return m_x; }
    int y() const { return m_y; }
    int width() const { return m_w; }
    int height() const { return m_h; }

private:
    int m_x, m_y, m_w, m_h;
    Color m_color;
};

// Single-line fixed-width bitmap text. `font` must be a 96-entry glyph table
// indexed by (ASCII - 0x20) -- see simple_font.h::kGlyphFont5x8 for one.
class TextWidget : public Widget {
public:
    TextWidget() = default;
    TextWidget(int x, int y, const char* text, Color fg, Color bg, const BitmapGlyph* font, uint8_t font_height)
        : m_x(x), m_y(y), m_text(text), m_fg(fg), m_bg(bg), m_font(font), m_font_height(font_height) {}

    void set_text(const char* text) { m_text = text; }
    void set_position(int x, int y) { m_x = x; m_y = y; }

    void draw(DisplayDriver& display) const override {
        int cx = m_x;
        for (const char* p = m_text; *p != '\0'; ++p) {
            uint8_t code = static_cast<uint8_t>(*p);
            if (code < 0x20 || code > 0x7E) continue;
            const BitmapGlyph& g = m_font[code - 0x20];
            if (g.width == 0 || g.data == nullptr) { cx += 1; continue; }
            for (uint8_t r = 0; r < g.height; ++r) {
                uint8_t row = g.data[r];
                for (uint8_t c = 0; c < g.width; ++c) {
                    bool on = (row >> (g.width - 1 - c)) & 1;
                    display.set_pixel(cx + c, m_y + r, on ? m_fg : m_bg);
                }
            }
            cx += g.width + 1; // 1-pixel letter spacing
        }
    }

private:
    int m_x = 0, m_y = 0;
    const char* m_text = "";
    Color m_fg = kColorWhite;
    Color m_bg = kColorBlack;
    const BitmapGlyph* m_font = nullptr;
    uint8_t m_font_height = 8;
};

// Vertical value bar (0.0-1.0) with green/yellow/red thresholds and a
// slowly-decaying "max-hold" cursor line -- generalizes a per-core CPU-load
// bar. Call set_value() once per frame before draw(); each call also
// advances the max-hold decay, so skipping frames changes its decay rate.
class BarWidget : public Widget {
public:
    BarWidget(int x, int y, int w, int h) : m_x(x), m_y(y), m_w(w), m_h(h) {}

    void set_value(float v) {
        m_value = std::clamp(v, 0.0f, 1.0f);
        m_max_hold = std::max(0.0f, m_max_hold - kMaxHoldDecay);
        if (m_value > m_max_hold) m_max_hold = m_value;
    }

    // Thresholds are the value at which the bar switches from the low to
    // mid color, and mid to high color, respectively.
    void set_thresholds(float mid_at, float high_at) { m_mid_at = mid_at; m_high_at = high_at; }
    void set_colors(Color low, Color mid, Color high) { m_low = low; m_mid = mid; m_high = high; }

    void draw(DisplayDriver& display) const override {
        int fill_h = static_cast<int>(m_h * m_value);
        if (fill_h > 0)
            display.fill_rect(m_x, m_y + m_h - fill_h, m_x + m_w - 1, m_y + m_h - 1, color_for(m_value));

        int hold_y = m_y + m_h - 1 - static_cast<int>(m_h * m_max_hold);
        display.draw_line(m_x, hold_y, m_x + m_w - 1, hold_y, color_for(m_max_hold));
    }

private:
    Color color_for(float v) const {
        if (v < m_mid_at) return m_low;
        if (v < m_high_at) return m_mid;
        return m_high;
    }

    int m_x, m_y, m_w, m_h;
    float m_value = 0.0f;
    float m_max_hold = 0.0f;
    float m_mid_at = 0.5f;
    float m_high_at = 0.8f;
    Color m_low = kColorGreen;
    Color m_mid = Color::from_rgb888(230, 126, 34);
    Color m_high = kColorRed;
    static constexpr float kMaxHoldDecay = 0.01f;
};

// Horizontal value bar (0.0-1.0) with the same green/yellow/red threshold
// scheme as BarWidget -- generalizes a disk-usage row. `thickness` is the
// bar's pixel height; pair with a TextWidget for a label, this widget draws
// only the bar itself.
class HBarWidget : public Widget {
public:
    HBarWidget(int x, int y, int w, int thickness) : m_x(x), m_y(y), m_w(w), m_thickness(thickness) {}

    void set_value(float v) { m_value = std::clamp(v, 0.0f, 1.0f); }
    void set_thresholds(float mid_at, float high_at) { m_mid_at = mid_at; m_high_at = high_at; }
    void set_colors(Color track, Color low, Color mid, Color high) {
        m_track = track; m_low = low; m_mid = mid; m_high = high;
    }

    void draw(DisplayDriver& display) const override {
        display.draw_thick_line(m_x, m_y, m_x + m_w - 1, m_y, m_thickness, m_track);
        int fill_w = std::max(1, static_cast<int>(m_w * m_value));
        display.draw_thick_line(m_x, m_y, m_x + fill_w - 1, m_y, m_thickness, color_for(m_value));
    }

private:
    Color color_for(float v) const {
        if (v < m_mid_at) return m_low;
        if (v < m_high_at) return m_mid;
        return m_high;
    }

    int m_x, m_y, m_w, m_thickness;
    float m_value = 0.0f;
    float m_mid_at = 0.75f;
    float m_high_at = 0.9f;
    Color m_track = Color::from_rgb888(0, 50, 100);
    Color m_low = kColorGreen;
    Color m_mid = Color::from_rgb888(230, 126, 34);
    Color m_high = kColorRed;
};

// Scrolling line-graph widget (fixed-capacity history, oldest values drop
// off the left) with a label shown when empty and the latest value shown as
// text once data exists -- generalizes a temperature/RAM history graph.
// push_value() is not thread/core safe; call it and draw() from the same
// core, same as every other widget here.
class LineGraphWidget : public Widget {
public:
    LineGraphWidget(int x, int y, int w, int h, double scale, Color color, const char* label,
                     const BitmapGlyph* font, uint8_t font_height)
        : m_x(x), m_y(y), m_w(w), m_h(h), m_scale(scale), m_color(color), m_label(label),
          m_font(font), m_font_height(font_height) {}

    // `value` is expected in [0, scale]; values outside that range still
    // plot, clipped by the axes' drawing bounds only insofar as draw_line()
    // itself clips (see DisplayDriver::set_pixel's bounds check).
    void push_value(double value) {
        m_values.push_back(value);
        while (m_values.size() > static_cast<size_t>(m_w))
            m_values.pop_front();
    }

    void draw(DisplayDriver& display) const override {
        display.draw_line(m_x, m_y, m_x, m_y + m_h - 1, kColorWhite);
        display.draw_line(m_x, m_y + m_h - 1, m_x + m_w - 1, m_y + m_h - 1, kColorWhite);

        if (m_values.empty()) {
            int label_len = 0;
            for (const char* p = m_label; *p; ++p) ++label_len;
            TextWidget label_widget(m_x + m_w / 2 - label_len * 3, m_y + m_h / 2, m_label,
                                     kColorWhite, kColorBlack, m_font, m_font_height);
            label_widget.draw(display);
            return;
        }

        int i = 0;
        int prev_x = 0, prev_y = 0;
        for (double v : m_values) {
            int x = m_x + i;
            int y = m_y + m_h - 1 - static_cast<int>((v / m_scale) * (m_h - 1));
            if (i > 0)
                display.draw_line(prev_x, prev_y, x, y, m_color);
            prev_x = x;
            prev_y = y;
            ++i;
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", m_values.back());
        TextWidget value_widget(m_x + 2, m_y + 2, buf, m_color, kColorBlack, m_font, m_font_height);
        value_widget.draw(display);
    }

private:
    int m_x, m_y, m_w, m_h;
    double m_scale;
    Color m_color;
    const char* m_label;
    const BitmapGlyph* m_font;
    uint8_t m_font_height;
    std::deque<double> m_values;
};

} // namespace pico_toolset
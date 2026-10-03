#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <span>

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
// indexed by (ASCII - 0x20) -- see simple_font.h::kGlyphFont5x8. `scale` is an
// integer pixel multiplier. With set_transparent(true) only "on" pixels are
// drawn (text over a graph/bar without a background box).
class TextWidget : public Widget {
public:
    TextWidget() = default;
    TextWidget(int x, int y, const char* text, Color fg, Color bg, const BitmapGlyph* font,
               uint8_t font_height, uint8_t scale = 1)
        : m_x(x), m_y(y), m_text(text), m_fg(fg), m_bg(bg), m_font(font),
          m_font_height(font_height), m_scale(scale) {}

    void set_text(const char* text) { m_text = text; }
    void set_position(int x, int y) { m_x = x; m_y = y; }
    void set_color(Color fg) { m_fg = fg; }
    void set_transparent(bool t) { m_transparent = t; }

    // Rendered size in pixels (including the 1px letter spacing, scaled).
    int text_width() const {
        int w = 0;
        for (const char* p = m_text; *p != '\0';) {
            const BitmapGlyph* g = next_glyph(p);
            if (!g) continue;
            w += (g->width == 0 || g->data == nullptr ? 1 : g->width + 1) * m_scale;
        }
        return w > 0 ? w - m_scale : 0;
    }
    int text_height() const { return m_font_height * m_scale; }

    // Moves the text so it is centered on (cx, cy).
    void set_centered(int cx, int cy) { set_position(cx - text_width() / 2, cy - text_height() / 2); }

    void draw(DisplayDriver& display) const override {
        int cx = m_x;
        for (const char* p = m_text; *p != '\0';) {
            const BitmapGlyph* gp = next_glyph(p);
            if (!gp) continue;
            const BitmapGlyph& g = *gp;
            if (g.width == 0 || g.data == nullptr) { cx += m_scale; continue; }
            for (uint8_t r = 0; r < g.height; ++r) {
                uint8_t row = g.data[r];
                for (uint8_t c = 0; c < g.width; ++c) {
                    bool on = (row >> (g.width - 1 - c)) & 1;
                    if (!on && m_transparent) continue;
                    Color col = on ? m_fg : m_bg;
                    if (m_scale == 1)
                        display.set_pixel(cx + c, m_y + r, col);
                    else
                        display.fill_rect(cx + c * m_scale, m_y + r * m_scale,
                                          cx + (c + 1) * m_scale - 1, m_y + (r + 1) * m_scale - 1, col);
                }
            }
            cx += (g.width + 1) * m_scale; // 1-pixel letter spacing
        }
    }

private:
    // Consumes one character at p and returns its glyph (nullptr if it has
    // none). Printable ASCII maps directly; the UTF-8 degree sign (0xC2 0xB0)
    // maps to font slot 95, which kGlyphFont5x8 fills with a degree glyph.
    const BitmapGlyph* next_glyph(const char*& p) const {
        uint8_t code = static_cast<uint8_t>(*p++);
        if (code == 0xC2 && static_cast<uint8_t>(*p) == 0xB0) { ++p; return &m_font[95]; }
        if (code < 0x20 || code > 0x7E) return nullptr;
        return &m_font[code - 0x20];
    }

    int m_x = 0, m_y = 0;
    const char* m_text = "";
    Color m_fg = kColorWhite;
    Color m_bg = kColorBlack;
    const BitmapGlyph* m_font = nullptr;
    uint8_t m_font_height = 8;
    uint8_t m_scale = 1;
    bool m_transparent = false;
};

// Blits an already-decoded RGB565 pixel array (row-major, `w*h` pixels) at
// a fixed position, one set_pixel() per pixel -- driver-agnostic, so it
// works over any DisplayDriver. This is the generic, chip-agnostic
// counterpart to the pixel-blit portion of what a driver-specific
// `bmp_show_image()` does; decoding a raw *file* format (e.g. a `.bmp`
// header) into such an array stays the caller's job, not this widget's.
// `pixels` must outlive every draw() call using it (stored by pointer, not
// copied, same convention as TextWidget's `text`).
class BitmapWidget : public Widget {
public:
    BitmapWidget(int x, int y, int w, int h, std::span<const uint16_t> pixels)
        : m_x(x), m_y(y), m_w(w), m_h(h), m_pixels(pixels) {}

    void draw(DisplayDriver& display) const override {
        if (static_cast<size_t>(m_w) * static_cast<size_t>(m_h) > m_pixels.size()) return;
        for (int y = 0; y < m_h; ++y)
            for (int x = 0; x < m_w; ++x)
                display.set_pixel(m_x + x, m_y + y, Color(m_pixels[static_cast<size_t>(y) * m_w + x]));
    }

private:
    int m_x, m_y, m_w, m_h;
    std::span<const uint16_t> m_pixels;
};

// Vertical value bar (0.0-1.0) with green/yellow/red thresholds and a
// slowly-decaying "max-hold" cursor line -- generalizes a per-core CPU-load
// bar. set_value() updates the bar (and raises the max-hold if exceeded);
// call tick() once per rendered frame to make the max-hold fall -- the decay
// is per frame, independent of how often new values arrive.
class BarWidget : public Widget {
public:
    BarWidget(int x, int y, int w, int h) : m_x(x), m_y(y), m_w(w), m_h(h) {}

    void set_value(float v) {
        m_value = std::clamp(v, 0.0f, 1.0f);
        if (m_value > m_max_hold) m_max_hold = m_value;
    }

    // Per-frame max-hold decay (never falls below the current value).
    // Returns true if the marker moved, i.e. the bar needs redrawing.
    bool tick() {
        const float old = m_max_hold;
        m_max_hold = std::max(m_value, m_max_hold - kMaxHoldDecay);
        return m_max_hold != old;
    }

    // Thresholds are the value at which the bar switches from the low to
    // mid color, and mid to high color, respectively.
    void set_thresholds(float mid_at, float high_at) { m_mid_at = mid_at; m_high_at = high_at; }
    void set_colors(Color low, Color mid, Color high) { m_low = low; m_mid = mid; m_high = high; }

    void draw(DisplayDriver& display) const override {
        int fill_h = std::max(1, static_cast<int>(m_h * m_value));
        display.fill_rect(m_x, m_y + m_h - fill_h, m_x + m_w - 1, m_y + m_h - 1, color_for(m_value));

        int hold_h = std::max(1, static_cast<int>(m_h * m_max_hold)) + 1;
        int hold_y = std::max(m_y, m_y + m_h - 1 - hold_h);
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
    Color m_low = Color::from_rgb888(32, 180, 96);
    Color m_mid = Color::from_rgb888(230, 126, 34);
    Color m_high = Color::from_rgb888(255, 20, 15);
    static constexpr float kMaxHoldDecay = 0.01f;
};

// Horizontal value bar (0.0-1.0) with the same green/yellow/red threshold
// scheme as BarWidget -- generalizes a disk-usage row. `thickness` is the
// bar's pixel height; pair with a TextWidget for a label (color() returns the
// bar's current fill color for that), this widget draws only the bar itself.
class HBarWidget : public Widget {
public:
    HBarWidget(int x, int y, int w, int thickness) : m_x(x), m_y(y), m_w(w), m_thickness(thickness) {}

    void set_value(float v) { m_value = std::clamp(v, 0.0f, 1.0f); }
    void set_thresholds(float mid_at, float high_at) { m_mid_at = mid_at; m_high_at = high_at; }
    void set_colors(Color track, Color low, Color mid, Color high) {
        m_track = track; m_low = low; m_mid = mid; m_high = high;
    }

    // Current fill color (low/mid/high according to the value).
    Color color() const { return color_for(m_value); }

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
    Color m_low = Color::from_rgb888(32, 180, 96);
    Color m_mid = Color::from_rgb888(230, 126, 34);
    Color m_high = Color::from_rgb888(255, 20, 15);
};

// Scrolling filled-area graph (fixed-capacity history of one column per
// value, oldest values drop off the left): each column is a dimmed fill from
// the baseline up to the value with a full-color pixel on top. A label is
// shown when empty; once data exists the latest value is drawn centered in
// the inverse color, over the graph. Generalizes a temperature/RAM history.
// push_value() is not thread/core safe; call it and draw() from the same
// core, same as every other widget here.
class LineGraphWidget : public Widget {
public:
    LineGraphWidget(int x, int y, int w, int h, double scale, Color color, const char* label,
                     const BitmapGlyph* font, uint8_t font_height, uint8_t text_scale = 1)
        : m_x(x), m_y(y), m_w(w), m_h(h), m_scale(scale), m_color(color), m_label(label),
          m_font(font), m_font_height(font_height), m_text_scale(text_scale) {}

    // Color of the label shown while the graph is empty (default white).
    void set_label_color(Color c) { m_label_color = c; }

    // `value` is expected in [0, scale]; it is clamped to the graph height.
    void push_value(double value) {
        m_values.push_back(value);
        while (m_values.size() > static_cast<size_t>(m_w))
            m_values.pop_front();
    }

    void draw(DisplayDriver& display) const override {
        const int cx = m_x + m_w / 2;
        if (m_values.empty()) {
            TextWidget label(0, 0, m_label, m_label_color, kColorBlack, m_font, m_font_height, m_text_scale);
            label.set_transparent(true);
            label.set_centered(cx, m_y + m_h / 2);
            label.draw(display);
            return;
        }

        const int base = m_y + m_h - 1;
        const Color fill = m_color.scaled(1, 3);
        int i = 0;
        for (double v : m_values) {
            int top = base - static_cast<int>(std::clamp(v / m_scale, 0.0, 1.0) * (m_h - 1));
            int x = m_x + i++;
            if (top < base) display.draw_line(x, base, x, top + 1, fill);
            display.set_pixel(x, top, m_color);
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", m_values.back());
        TextWidget value(0, 0, buf, m_color.inverted(), kColorBlack, m_font, m_font_height, m_text_scale);
        value.set_transparent(true);
        value.set_centered(cx, m_y + m_h / 2);
        value.draw(display);
    }

private:
    int m_x, m_y, m_w, m_h;
    double m_scale;
    Color m_color;
    const char* m_label;
    const BitmapGlyph* m_font;
    uint8_t m_font_height;
    uint8_t m_text_scale;
    Color m_label_color = kColorWhite;
    std::deque<double> m_values;
};

} // namespace pico_toolset
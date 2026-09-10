#pragma once

#include <cstdint>

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

} // namespace pico_toolset
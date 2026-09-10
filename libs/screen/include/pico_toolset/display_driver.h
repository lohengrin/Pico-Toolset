#pragma once

#include <cstdint>

namespace pico_toolset {

// RGB565 convenience color helper (5-6-5 packed).
struct Color {
    uint16_t rgb565;

    constexpr Color() = default;
    constexpr Color(uint16_t v) : rgb565(v) {}
    constexpr static Color from_rgb888(uint8_t r, uint8_t g, uint8_t b) {
        return Color(static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
    }
};

constexpr Color kColorBlack = Color(0x0000);
constexpr Color kColorWhite = Color(0xFFFF);
constexpr Color kColorRed = Color(0xF800);
constexpr Color kColorGreen = Color(0x07E0);
constexpr Color kColorBlue = Color(0x001F);

// Abstract pixel-addressable display. Everything the Screen composition and
// the widgets need is a set_pixel + clear; drivers may implement fill/blit
// more efficiently and expose lower-level tricks via dynamic_cast.
class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;

    [[nodiscard]] virtual int width() const = 0;
    [[nodiscard]] virtual int height() const = 0;

    // Pixel at (x, y), clamps coordinates to the display area.
    virtual void set_pixel(int x, int y, Color color) = 0;

    // Fills a horizontal band [x0..x1]x[y0..y1] with one color. Default
    // implementation loops set_pixel(); drivers should override for speed.
    virtual void fill_rect(int x0, int y0, int x1, int y1, Color color) {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                set_pixel(x, y, color);
    }

    // Push any pending buffered changes to the physical panel.
    virtual void flush() {}

    // Optional: set panel backlight (0-255). Default no-op.
    virtual void set_backlight(uint8_t /**/ ) {}
};

} // namespace pico_toolset
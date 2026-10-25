#pragma once

// Row conversion / scaling for 8-bit indexed frames shown on a 480-wide ST7701 (Presto). Pure
// arithmetic with no hardware dependency, kept in a header so the RAM-resident scanout callback
// (st7701_indexed.cpp) inlines it and the host tests run it.
//
// 320 -> 480 is an exact 1.5x nearest-neighbour scale: source pixels (a b) become (a a b), and the
// vertical mapping is panel row y -> source row y * 2 / 3 (source rows 2k -> panel rows 3k, 3k+1;
// 2k+1 -> 3k+2). Pointers are 4-byte aligned; the conversions work in 32-bit words.
#include <cstdint>

#if defined(__GNUC__)
#define PICO_TOOLSET_ST7701_INLINE inline __attribute__((always_inline))
#else
#define PICO_TOOLSET_ST7701_INLINE inline
#endif

namespace pico_toolset {

constexpr int indexed_source_row_1p5(int panel_row) { return panel_row * 2 / 3; }

// 320 index bytes -> 480 index bytes (the palette-framebuffer path).
PICO_TOOLSET_ST7701_INLINE void expand_indexed_row_1p5(uint8_t* dst, const uint8_t* src) {
    const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
    uint32_t* d = reinterpret_cast<uint32_t*>(dst);
    for (int i = 0; i < 320 / 8; ++i) {
        const uint32_t w0 = s[2 * i], w1 = s[2 * i + 1];
        const uint32_t a = w0 & 0xFF, b = (w0 >> 8) & 0xFF, c = (w0 >> 16) & 0xFF, e = w0 >> 24;
        const uint32_t f = w1 & 0xFF, g = (w1 >> 8) & 0xFF, h = (w1 >> 16) & 0xFF, k = w1 >> 24;
        // output bytes: a a b c | c e f f | g h h k  (pairs (a,b),(c,e),(f,g),(h,k))
        d[3 * i]     = a | (a << 8) | (b << 16) | (c << 24);
        d[3 * i + 1] = c | (e << 8) | (f << 16) | (f << 24);
        d[3 * i + 2] = g | (h << 8) | (h << 16) | (k << 24);
    }
}

// 480 index bytes -> 480 pixels through a 256-entry 16-bit LUT (two pixels per output word).
PICO_TOOLSET_ST7701_INLINE void indexed_row_to_pixels(uint32_t* dst, const uint8_t* src, const uint16_t* lut) {
    const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
    for (int i = 0; i < 480 / 4; ++i) {
        const uint32_t w = s[i];
        dst[2 * i]     = lut[w & 0xFF] | (static_cast<uint32_t>(lut[(w >> 8) & 0xFF]) << 16);
        dst[2 * i + 1] = lut[(w >> 16) & 0xFF] | (static_cast<uint32_t>(lut[w >> 24]) << 16);
    }
}

// 320 index bytes -> 480 pixels (1.5x), same LUT convention.
PICO_TOOLSET_ST7701_INLINE void indexed_row_to_pixels_1p5(uint32_t* dst, const uint8_t* src, const uint16_t* lut) {
    const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
    for (int i = 0; i < 320 / 4; ++i) {
        const uint32_t w = s[i];
        const uint32_t a = lut[w & 0xFF], b = lut[(w >> 8) & 0xFF];
        const uint32_t c = lut[(w >> 16) & 0xFF], e = lut[w >> 24];
        dst[3 * i]     = a | (a << 16);
        dst[3 * i + 1] = b | (c << 16);
        dst[3 * i + 2] = c | (e << 16);
    }
}

} // namespace pico_toolset

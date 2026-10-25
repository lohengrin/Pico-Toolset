#pragma once

// Scanline565 line source for an 8-bit indexed frame (a game, an emulator): the application keeps a
// palette-indexed frame buffer in SRAM and this object turns it into the panel's RGB565 lines one at
// a time, just ahead of the beam (the St7701 `line_callback`), with a 256-entry colour table, an
// optional 1.5x nearest-neighbour upscale and an optional "overlay" of rows read from another buffer
// (for a tear-free copy of part of the frame). No framebuffer in the panel driver, ~0.5 KB of table.
//
//   St7701IndexedSource source;
//   source.set_entry(i, r, g, b);            // or set_palette(); the table starts black
//   source.set_source(frame, 480);           // 480 wide (1:1) or 320 wide (1.5x); nullptr = black
//   St7701Config cfg = configs::st7701::kPimoroniPrestoFullRes;
//   cfg.visible_height = 300;
//   source.attach(cfg);                      // format = Scanline565, line_callback = this source
//   lcd.init(cfg);  /* on core1 */  while (true) lcd.scanline_task();
//
// The callback runs on the scanout core from RAM: the frame, the overlay and this object must be in
// SRAM, and the setters may be called from the other core (pointer stores are atomic; a change takes
// effect on the next line).
#if PICO_RP2350

#include <cstdint>

#include "pico_toolset/st7701.h"

namespace pico_toolset {

class St7701IndexedSource {
public:
    // Colour table entry `i` (8-bit RGB, quantised to RGB565 and byte-swapped for the panel bus).
    void set_entry(int i, uint8_t r, uint8_t g, uint8_t b) {
        m_lut[i & 255] = st7701_encode(static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
    }

    // The frame: `width` (480 or 320) index bytes per row, rows consecutive, 4-byte aligned.
    // 320 is upscaled 1.5x (panel row y reads source row y * 2 / 3). nullptr shows black.
    void set_source(const uint8_t* pixels, int width) {
        m_width = width;
        m_source = pixels;
    }

    // 480-wide frames only: panel rows [first_row, first_row + count) are read from `rows`
    // (row r at rows + (r - first_row) * 480) instead of the frame. count = 0 turns it off.
    void set_overlay(const uint8_t* rows, int first_row, int count) {
        m_overlay = rows;
        m_overlay_first = first_row;
        m_overlay_count = count;
    }

    // Selects Scanline565 and installs this source as the line callback (the object must outlive
    // the panel driver). Set cfg.visible_height yourself.
    void attach(St7701Config& cfg) {
        cfg.format = St7701Format::Scanline565;
        cfg.line_callback = &St7701IndexedSource::line_callback;
        cfg.line_user = this;
    }

    // St7701LineCallback; RAM-resident (st7701_indexed.cpp).
    static void line_callback(int y, uint16_t* line, void* user);

private:
    alignas(4) uint16_t m_lut[256] = {};
    const uint8_t* volatile m_source = nullptr;
    volatile int m_width = 480;
    const uint8_t* volatile m_overlay = nullptr;
    volatile int m_overlay_first = 0;
    volatile int m_overlay_count = 0;
};

} // namespace pico_toolset

#endif // PICO_RP2350

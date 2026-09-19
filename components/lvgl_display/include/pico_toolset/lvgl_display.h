#pragma once

#include "pico_toolset/display_panel.h"
#include "pico_toolset/touch_panel.h"

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace pico_toolset {

struct LvglDisplayConfig {
    // Partial-render tile buffer, RGB565, caller-owned (static SRAM, PSRAM,
    // ...). A few dozen rows of the panel width is typical.
    uint16_t* draw_buffer = nullptr;
    size_t draw_buffer_pixels = 0;
};

// Linear touch calibration: raw controller counts -> panel pixels.
struct LvglTouchCalibration {
    bool swap_axes = false;
    uint16_t raw_h_min = 0, raw_h_max = 4095; // horizontal (after optional swap)
    uint16_t raw_v_min = 0, raw_v_max = 4095;
};

// Bridges any DisplayPanel (SPI TFT drivers) and TouchPanel to LVGL v9:
// partial-render display with a flush callback, and a pointer indev. The
// consumer owns lv_conf.h (see cmake/pico_lvgl.cmake).
class LvglDisplayAdapter {
public:
    // Calls lv_init() and creates the display. Returns false if the config
    // has no draw buffer.
    bool init(DisplayPanel& panel, const LvglDisplayConfig& config);

    // Framebuffer variant: renders into a caller-owned RGB565 framebuffer of
    // width x height pixels (native byte order, e.g. one a DVI/HDMI scanout
    // engine re-reads continuously) instead of pushing to a DisplayPanel.
    bool init_framebuffer(uint16_t* framebuffer, int width, int height, const LvglDisplayConfig& config);

    // Same, for an 8-bit RRRGGGBB (RGB332) framebuffer -- half the memory,
    // for RAM-tight chips (RP2040) driving DVI in 8bpp. LVGL still renders
    // RGB565 into the tile buffer; the flush converts.
    bool init_framebuffer_rgb332(uint8_t* framebuffer, int width, int height, const LvglDisplayConfig& config);

    // Registers a pointer input device fed by `touch`. `panel` size (from
    // init) is the mapping target.
    void add_touch(TouchPanel& touch, const LvglTouchCalibration& calibration);

    // Advances LVGL's tick from the pico clock and runs lv_timer_handler().
    // Call from the main loop as often as possible.
    void tick();

    // Called after every flushed tile and every tick(): lets the consumer
    // keep servicing something time-sensitive (e.g. a USB device stack)
    // while a large redraw is in progress.
    void set_idle_hook(void (*hook)()) { s_idle_hook = hook; }

    [[nodiscard]] lv_display_t* display() const { return m_display; }
    static inline void (*s_idle_hook)() = nullptr;

private:
    lv_display_t* m_display = nullptr;
    DisplayPanel* m_panel = nullptr;
    int m_width = 0, m_height = 0;
    uint32_t m_last_tick_ms = 0;
};

} // namespace pico_toolset

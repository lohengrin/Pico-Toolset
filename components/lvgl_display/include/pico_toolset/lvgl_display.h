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

    // Registers a pointer input device fed by `touch`. `panel` size (from
    // init) is the mapping target.
    void add_touch(TouchPanel& touch, const LvglTouchCalibration& calibration);

    // Advances LVGL's tick from the pico clock and runs lv_timer_handler().
    // Call from the main loop as often as possible.
    void tick();

    [[nodiscard]] lv_display_t* display() const { return m_display; }

private:
    lv_display_t* m_display = nullptr;
    DisplayPanel* m_panel = nullptr;
    uint32_t m_last_tick_ms = 0;
};

} // namespace pico_toolset

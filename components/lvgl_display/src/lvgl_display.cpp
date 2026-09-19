#include "pico_toolset/lvgl_display.h"

#include "pico/stdlib.h"

#include <algorithm>
#include <cstring>
#include <span>

namespace pico_toolset {

namespace {

struct TouchCtx {
    TouchPanel* touch = nullptr;
    LvglTouchCalibration cal;
    int width = 0, height = 0;
} g_touch;

void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    auto& panel = *static_cast<DisplayPanel*>(lv_display_get_user_data(disp));
    const size_t count = static_cast<size_t>(area->x2 - area->x1 + 1) * static_cast<size_t>(area->y2 - area->y1 + 1);
    auto* pixels = reinterpret_cast<uint16_t*>(px_map);

    // DisplayPanel::write_pixels() wants big-endian wire order; LVGL's RGB565
    // buffer is native little-endian. Swapped in place (LVGL's own buffer,
    // not reused until the next render).
    for (size_t i = 0; i < count; ++i) {
        const uint16_t v = pixels[i];
        pixels[i] = static_cast<uint16_t>((v << 8) | (v >> 8));
    }

    panel.set_window(area->x1, area->y1, area->x2, area->y2);
    panel.write_pixels(std::span<const uint16_t>(pixels, count));
    panel.end_write();
    lv_display_flush_ready(disp);
    if (LvglDisplayAdapter::s_idle_hook) LvglDisplayAdapter::s_idle_hook();
}

struct Framebuffer {
    uint16_t* pixels = nullptr;
    int width = 0, height = 0;
} g_fb;

void flush_fb_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    const int x1 = std::max<int>(area->x1, 0), y1 = std::max<int>(area->y1, 0);
    const int x2 = std::min<int>(area->x2, g_fb.width - 1), y2 = std::min<int>(area->y2, g_fb.height - 1);
    if (x2 >= x1 && y2 >= y1) {
        const size_t area_w = static_cast<size_t>(area->x2 - area->x1 + 1); // px_map's stride
        const auto* src = reinterpret_cast<const uint16_t*>(px_map) + (x1 - area->x1) + static_cast<size_t>(y1 - area->y1) * area_w;
        uint16_t* dst = g_fb.pixels + static_cast<size_t>(y1) * g_fb.width + x1;
        const size_t row_bytes = static_cast<size_t>(x2 - x1 + 1) * sizeof(uint16_t);
        for (int y = y1; y <= y2; ++y) {
            std::memcpy(dst, src, row_bytes);
            src += area_w;
            dst += g_fb.width;
        }
    }
    lv_display_flush_ready(disp);
    if (LvglDisplayAdapter::s_idle_hook) LvglDisplayAdapter::s_idle_hook();
}

int32_t map(double v, double in_min, double in_max, double out_max) {
    const double t = (v - in_min) / (in_max - in_min) * out_max;
    return static_cast<int32_t>(std::clamp(t, 0.0, out_max));
}

void touch_read_cb(lv_indev_t*, lv_indev_data_t* data) {
    const TouchSample raw = g_touch.touch->read();
    if (!raw.pressed) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    const auto& c = g_touch.cal;
    const uint16_t h = c.swap_axes ? raw.raw_y : raw.raw_x;
    const uint16_t v = c.swap_axes ? raw.raw_x : raw.raw_y;
    data->point.x = map(h, c.raw_h_min, c.raw_h_max, g_touch.width - 1);
    data->point.y = map(v, c.raw_v_min, c.raw_v_max, g_touch.height - 1);
    data->state = LV_INDEV_STATE_PRESSED;
}

} // namespace

bool LvglDisplayAdapter::init(DisplayPanel& panel, const LvglDisplayConfig& config) {
    if (!config.draw_buffer || config.draw_buffer_pixels == 0) return false;

    lv_init();
    m_panel = &panel;
    m_width = panel.width();
    m_height = panel.height();
    m_display = lv_display_create(panel.width(), panel.height());
    lv_display_set_user_data(m_display, &panel);
    lv_display_set_color_format(m_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(m_display, config.draw_buffer, nullptr, config.draw_buffer_pixels * sizeof(uint16_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(m_display, flush_cb);
    m_last_tick_ms = to_ms_since_boot(get_absolute_time());
    return true;
}

bool LvglDisplayAdapter::init_framebuffer(uint16_t* framebuffer, int width, int height, const LvglDisplayConfig& config) {
    if (!framebuffer || !config.draw_buffer || config.draw_buffer_pixels == 0) return false;
    lv_init();
    g_fb = {framebuffer, width, height};
    m_width = width;
    m_height = height;
    m_display = lv_display_create(width, height);
    lv_display_set_color_format(m_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(m_display, config.draw_buffer, nullptr, config.draw_buffer_pixels * sizeof(uint16_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(m_display, flush_fb_cb);
    m_last_tick_ms = to_ms_since_boot(get_absolute_time());
    return true;
}

void LvglDisplayAdapter::add_touch(TouchPanel& touch, const LvglTouchCalibration& calibration) {
    g_touch = {&touch, calibration, m_width, m_height};
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}

void LvglDisplayAdapter::tick() {
    const uint32_t now = to_ms_since_boot(get_absolute_time());
    lv_tick_inc(now - m_last_tick_ms);
    m_last_tick_ms = now;
    lv_timer_handler();
    if (s_idle_hook) s_idle_hook();
}

} // namespace pico_toolset

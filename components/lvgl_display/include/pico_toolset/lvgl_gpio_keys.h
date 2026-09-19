#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace pico_toolset {

struct LvglGpioKey {
    uint8_t pin;
    uint32_t lv_key; // LV_KEY_PREV / LV_KEY_NEXT / LV_KEY_ENTER / LV_KEY_ESC ...
};

struct LvglGpioKeysConfig {
    const LvglGpioKey* keys = nullptr;
    size_t key_count = 0;
    bool active_low = true; // pressed = pin low (internal pull-up enabled)
};

// Momentary buttons as an LVGL keypad: creates a new lv_group, makes it the
// default and binds a keypad indev to it -- call BEFORE building the widgets
// that should be navigable. The config's key array must outlive the UI.
void lvgl_gpio_keys_init(const LvglGpioKeysConfig& config);

} // namespace pico_toolset

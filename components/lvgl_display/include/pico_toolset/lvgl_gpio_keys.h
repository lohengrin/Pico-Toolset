#pragma once

#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace pico_toolset {

struct LvglGpioKey {
    uint8_t pin;
    uint32_t lv_key; // LV_KEY_PREV / LV_KEY_NEXT / LV_KEY_ENTER / LV_KEY_ESC ...
};

enum class LvglGpioKeyPolarity {
    kActiveLow,  // pressed = pin low (internal pull-up enabled)
    kActiveHigh, // pressed = pin high (internal pull-down enabled)
    // Wiring-independent: at init, with every button released, record each
    // pin's idle reading with the internal pull-up and with the pull-down; a
    // button counts as pressed whenever that pair of readings changes (pressed
    // to GND reads low with both pulls, pressed to 3V3 reads high with both,
    // a released pin either follows the pulls or keeps the level its external
    // resistor sets). Pull direction is alternated on every poll.
    kAuto,
};

struct LvglGpioKeysConfig {
    const LvglGpioKey* keys = nullptr;
    size_t key_count = 0;
    LvglGpioKeyPolarity polarity = LvglGpioKeyPolarity::kActiveLow;
};

// Momentary buttons as an LVGL keypad: creates a new lv_group, makes it the
// default and binds a keypad indev to it -- call BEFORE building the widgets
// that should be navigable. The config's key array must outlive the UI.
void lvgl_gpio_keys_init(const LvglGpioKeysConfig& config);

} // namespace pico_toolset

#pragma once

// Known-good Ft6236Config presets for specific boards.

#include "pico_toolset/ft6236.h"

namespace pico_toolset::configs::ft6236 {

// Pimoroni Presto's built-in touch layer, I2C1 on GPIO30/31, INT on GPIO32,
// at the half-resolution (240x240) default; set width/height to 480 for
// full-res. Address 0x48 and the pins come from Pimoroni's Presto firmware
// (touch.py) and pico-sdk's pimoroni_presto.h; Validated on real hardware by examples/presto. The touch reset line is GPIO42
// (PRESTO_TOUCH_RESET_PIN) -- Pimoroni's firmware never drives it.
inline const Ft6236Config kPimoroniPrestoHalfRes = {
    .i2c = i2c1,
    .pin_sda = 30,
    .pin_scl = 31,
    .pin_int = 32,
    .addr = 0x48,
    .width = 240,
    .height = 240,
    .rotate_180 = false,
};

inline const Ft6236Config kPimoroniPrestoFullRes = {
    .i2c = i2c1,
    .pin_sda = 30,
    .pin_scl = 31,
    .pin_int = 32,
    .addr = 0x48,
    .width = 480,
    .height = 480,
    .rotate_180 = false,
};

} // namespace pico_toolset::configs::ft6236

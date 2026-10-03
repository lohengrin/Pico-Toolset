#pragma once

// Known-good RgbLedConfig presets for specific boards.

#include "pico_toolset/rgb_led.h"

namespace pico_toolset::configs::rgb_led {

// Pimoroni Pico Display Pack's RGB LED (R=GP6, G=GP7, B=GP8, active-low).
// Same wiring as the board's own library; validated on real hardware through
// the PiCoMonitor firmware.
inline constexpr RgbLedConfig kPimoroniPicoDisplayPack = {
    .pin_r = 6,
    .pin_g = 7,
    .pin_b = 8,
    .active_low = true,
};

} // namespace pico_toolset::configs::rgb_led

#pragma once

// Known-good ButtonsConfig presets for specific boards.

#include "pico_toolset/reset_buttons.h"

namespace pico_toolset::configs::buttons {

// Pimoroni Pico Display Pack's four buttons, active-low (pull-up, closing to
// GND). Index order is the physical corner order: 0 = A (top-left), 1 = B
// (bottom-left), 2 = X (top-right), 3 = Y (bottom-right); pins GP12..GP15.
// Validated on real hardware through the PiCoMonitor firmware.
inline constexpr ButtonsConfig kPimoroniPicoDisplayPack = {
    .pins = {12, 13, 14, 15, 0, 0, 0, 0},
    .count = 4,
    .active_low = true,
    .debounce_frames = 3,
};

} // namespace pico_toolset::configs::buttons

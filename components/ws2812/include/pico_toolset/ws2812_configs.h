#pragma once

// Known-good Ws2812Config presets for specific boards.

#include "pico_toolset/ws2812.h"

namespace pico_toolset::configs::ws2812 {

// Pimoroni Presto's 7 backlight/ambient RGB LEDs (GRB order) on GPIO33, on
// PIO0 state machine 3 like Pimoroni's firmware (PIO1 belongs to the LCD
// scanout). GPIO33 lies outside the PIO's default 0-31 window, so gpio_base
// moves PIO0 to 16-47. Derived from Pimoroni's Presto firmware, not yet
// re-run from this repo on hardware.
inline const Ws2812Config kPimoroniPresto = {
    .pin = 33,
    .count = 7,
    .pio = pio0,
    .sm = 3,
    .order = Ws2812Order::GRB,
    .gpio_base = 16,
};

} // namespace pico_toolset::configs::ws2812

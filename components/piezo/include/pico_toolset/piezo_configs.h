#pragma once

// Known-good PiezoConfig presets for specific boards.

#include "pico_toolset/piezo.h"

namespace pico_toolset::configs::piezo {

// Pimoroni Presto's on-board piezo speaker, driven through a transistor from
// GPIO43 (PRESTO_BUZZER_PIN). Derived from Pimoroni's Presto firmware
// (examples/buzzer.py), not yet re-run from this repo on hardware.
inline constexpr PiezoConfig kPimoroniPresto = {
    .pin = 43,
};

} // namespace pico_toolset::configs::piezo
